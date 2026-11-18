// DS 用の BGM：原作の General MIDI の曲を Windows 自身の MIDI シンセサイザー（MIDI マッパー、
// 普通は Microsoft GS Wavetable Synth）で再生し、既定の音声出力から録音して（WASAPI ループバック）、
// DS のサウンドハードウェア用に IMA-ADPCM で符号化する。
// 描画は実時間なので、PC でほかに鳴っているものも録音される。
//
// 音楽のファイル（.bin、「RMU1」）：
//   char magic[4] 「RMU1」。u16 count, u16 rate（Hz）
//   曲ごとに：u32 bytes。u8 data[bytes]（DS の IMA-ADPCM：s16 最初のサンプル、u8 ステップの番号、
//   u8 0、それから4ビットのコード、下位のニブルが先。bytes は4の倍数）、先頭からループ
//
// 録音は各曲の終わりを過ぎても続く。その尾（残響、リリース）を先頭に混ぜるので、ループに切れ目が無い。
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;

namespace Re4Zeebo
{
    public static class MidiFile
    {
        // フォーマット 0/1 のファイルのチャンネルメッセージを（秒単位の時刻, パックしたショートメッセージ）で。
        public static List<KeyValuePair<double, uint>> Events(byte[] b, out double length)
        {
            if (b.Length < 14 || Encoding.ASCII.GetString(b, 0, 4) != "MThd") throw new InvalidDataException("not a MIDI file");
            int ntr = b[10] << 8 | b[11], div = b[12] << 8 | b[13];
            if ((div & 0x8000) != 0) throw new InvalidDataException("SMPTE time division not supported");
            var raw = new List<Tuple<long, int, uint>>();   // ティック、順番、メッセージ（テンポの変更は 0xFF000000 | テンポ）
            int p = 14, order = 0;
            long maxTick = 0;
            for (int t = 0; t < ntr && p + 8 <= b.Length; t++)
            {
                int len = b[p + 4] << 24 | b[p + 5] << 16 | b[p + 6] << 8 | b[p + 7];
                int q = p + 8, end = Math.Min(b.Length, q + len);
                p = q + len;
                long tick = 0;
                int run = 0;
                while (q < end)
                {
                    tick += VarLen(b, ref q);
                    int st = b[q];
                    if (st == 0xFF)
                    {
                        int type = b[q + 1]; q += 2;
                        int l = VarLen(b, ref q);
                        if (type == 0x51 && l == 3) raw.Add(Tuple.Create(tick, order++, 0xFF000000u | (uint)(b[q] << 16 | b[q + 1] << 8 | b[q + 2])));
                        q += l;
                        continue;
                    }
                    if (st == 0xF0 || st == 0xF7) { q++; int l = VarLen(b, ref q); q += l; continue; }
                    if ((st & 0x80) != 0) { run = st; q++; }
                    int hi = run & 0xF0;
                    int d1 = b[q++], d2 = hi == 0xC0 || hi == 0xD0 ? 0 : b[q++];
                    raw.Add(Tuple.Create(tick, order++, (uint)(run | d1 << 8 | d2 << 16)));
                    maxTick = Math.Max(maxTick, tick);
                }
            }
            raw.Sort((x, y) => x.Item1 != y.Item1 ? x.Item1.CompareTo(y.Item1) : x.Item2.CompareTo(y.Item2));
            var o = new List<KeyValuePair<double, uint>>();
            double sec = 0; long last = 0; int tempo = 500000;
            foreach (var e in raw)
            {
                sec += (e.Item1 - last) * (double)tempo / div / 1e6;
                last = e.Item1;
                if ((e.Item3 & 0xFF000000) == 0xFF000000) tempo = (int)(e.Item3 & 0xFFFFFF);
                else o.Add(new KeyValuePair<double, uint>(sec, e.Item3));
            }
            length = sec + (maxTick - last) * (double)tempo / div / 1e6;
            // 最後のイベントがある小節の終わりまで切り上げる（最後のテンポで 4/4）。最後の音が早く終わっても
            // ループが拍を保つように。
            double bar = 4.0 * tempo / 1e6;
            length = Math.Ceiling(length / bar - 0.01) * bar;
            return o;
        }

        static int VarLen(byte[] b, ref int p)
        {
            int v = 0; byte c;
            do { c = b[p++]; v = (v << 7) | (c & 0x7F); } while ((c & 0x80) != 0);
            return v;
        }
    }

    public class Loopback
    {
        [ComImport, Guid("BCDE0395-E52F-467C-8E3D-C4579291692E")] class MMDeviceEnumeratorCom { }
        [Guid("A95664D2-9614-4F35-A746-DE8DB63617E6"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
        interface IMMDeviceEnumerator { int NotImpl1(); [PreserveSig] int GetDefaultAudioEndpoint(int dataFlow, int role, out IMMDevice dev); }
        [Guid("D666063F-1587-4E43-81F1-B948E807363F"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
        interface IMMDevice { [PreserveSig] int Activate(ref Guid iid, int ctx, IntPtr p, [MarshalAs(UnmanagedType.IUnknown)] out object o); }
        [Guid("1CB9AD4C-DBFA-4c32-B178-C2F568A703B2"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
        interface IAudioClient
        {
            [PreserveSig] int Initialize(int shareMode, int flags, long bufDur, long period, IntPtr fmt, IntPtr session);
            [PreserveSig] int GetBufferSize(out uint n);
            [PreserveSig] int GetStreamLatency(out long l);
            [PreserveSig] int GetCurrentPadding(out uint n);
            [PreserveSig] int IsFormatSupported(int m, IntPtr f, out IntPtr c);
            [PreserveSig] int GetMixFormat(out IntPtr fmt);
            [PreserveSig] int GetDevicePeriod(out long a, out long b);
            [PreserveSig] int Start();
            [PreserveSig] int Stop();
            [PreserveSig] int Reset();
            [PreserveSig] int SetEventHandle(IntPtr h);
            [PreserveSig] int GetService(ref Guid iid, [MarshalAs(UnmanagedType.IUnknown)] out object o);
        }
        [Guid("C8ADBD64-E71E-48a0-A4DE-185C395CD317"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
        interface IAudioCaptureClient
        {
            [PreserveSig] int GetBuffer(out IntPtr data, out uint frames, out uint flags, out long pos, out long qpc);
            [PreserveSig] int ReleaseBuffer(uint frames);
            [PreserveSig] int GetNextPacketSize(out uint n);
        }

        IAudioClient client;
        IAudioCaptureClient cap;
        public int Rate, Channels, Bits;
        public readonly List<float> Mono = new List<float>();
        volatile bool stop;
        Thread thread;
        readonly Stopwatch clock = new Stopwatch();
        public long StartTicks;   // 最初に録ったサンプルでのストップウォッチのティック

        public Loopback()
        {
            var en = (IMMDeviceEnumerator)new MMDeviceEnumeratorCom();
            IMMDevice dev;
            if (en.GetDefaultAudioEndpoint(0, 0, out dev) != 0) throw new InvalidOperationException("no audio output device");
            Guid iid = typeof(IAudioClient).GUID; object o;
            dev.Activate(ref iid, 23, IntPtr.Zero, out o);
            client = (IAudioClient)o;
            IntPtr fmt; client.GetMixFormat(out fmt);
            Channels = Marshal.ReadInt16(fmt, 2); Rate = Marshal.ReadInt32(fmt, 4); Bits = Marshal.ReadInt16(fmt, 14);
            int hr = client.Initialize(0, 0x00020000 /* AUDCLNT_STREAMFLAGS_LOOPBACK */, 10000000, 0, fmt, IntPtr.Zero);
            if (hr != 0) throw new InvalidOperationException("loopback capture failed: 0x" + hr.ToString("x"));
            Guid iidCap = typeof(IAudioCaptureClient).GUID;
            client.GetService(ref iidCap, out o);
            cap = (IAudioCaptureClient)o;
        }

        public Stopwatch Clock { get { return clock; } }

        public void Start()
        {
            clock.Start();
            client.Start();
            thread = new Thread(Run) { IsBackground = true, Priority = ThreadPriority.Highest };
            thread.Start();
        }

        public void Stop() { stop = true; thread.Join(); client.Stop(); }

        // パケットはデバイスの位置で置くので、無音の区間（パケットが無い）も時間どおりに残る。
        void Run()
        {
            long first = -1;
            while (!stop)
            {
                Thread.Sleep(5);
                uint n;
                while (cap.GetNextPacketSize(out n) == 0 && n > 0)
                {
                    IntPtr data; uint got, flags; long pos, qpc;
                    cap.GetBuffer(out data, out got, out flags, out pos, out qpc);
                    if (first < 0)
                    {
                        first = pos;
                        // qpc はパフォーマンスカウンターの 100 ns 単位。ストップウォッチも同じカウンターを使う。
                        StartTicks = (long)(qpc / 1e7 * Stopwatch.Frequency);
                    }
                    long at = pos - first;
                    while (Mono.Count < at) Mono.Add(0);
                    for (int i = 0; i < got; i++)
                    {
                        float sum = 0;
                        if ((flags & 2) == 0)
                            for (int c = 0; c < Channels; c++)
                                sum += Bits == 32 ? BitConverter.ToSingle(BitConverter.GetBytes(Marshal.ReadInt32(data, (i * Channels + c) * 4)), 0)
                                                  : Marshal.ReadInt16(data, (i * Channels + c) * 2) / 32768f;
                        float v = sum / Channels;
                        if (at + i < Mono.Count) Mono[(int)(at + i)] = v; else Mono.Add(v);
                    }
                    cap.ReleaseBuffer(got);
                }
            }
        }
    }

    public static partial class Tool
    {
        [DllImport("winmm.dll")] static extern int midiOutOpen(out IntPtr h, int dev, IntPtr cb, IntPtr inst, int flags);
        [DllImport("winmm.dll")] static extern int midiOutShortMsg(IntPtr h, uint msg);
        [DllImport("winmm.dll")] static extern int midiOutReset(IntPtr h);
        [DllImport("winmm.dll")] static extern int midiOutClose(IntPtr h);
        [DllImport("winmm.dll")] static extern int timeBeginPeriod(int ms);
        [DllImport("winmm.dll")] static extern int timeEndPeriod(int ms);

        const double TailSeconds = 3.0;

        // 出力を録音しながら MIDI マッパーで1曲を再生する。デバイスのミックスのレートでループ
        // （length 秒、尾は先頭に折り込む）を返す。
        static float[] RenderMidi(byte[] mid, out int rate, out double length)
        {
            var events = MidiFile.Events(mid, out length);
            IntPtr h;
            if (midiOutOpen(out h, -1, IntPtr.Zero, IntPtr.Zero, 0) != 0) throw new InvalidOperationException("no MIDI output");
            timeBeginPeriod(1);
            try
            {
                for (int ch = 0; ch < 16; ch++)   // コントローラーをリセット、全部の音を止める
                {
                    midiOutShortMsg(h, (uint)(0xB0 | ch | 121 << 8));
                    midiOutShortMsg(h, (uint)(0xB0 | ch | 123 << 8));
                }
                Thread.Sleep(300);
                var rec = new Loopback();
                rec.Start();
                Thread.Sleep(500);   // 最初の音の前に録音が動いている
                long t0 = rec.Clock.ElapsedTicks;
                foreach (var e in events)
                {
                    long due = t0 + (long)(e.Key * Stopwatch.Frequency);
                    while (rec.Clock.ElapsedTicks < due - Stopwatch.Frequency / 500) Thread.Sleep(1);
                    while (rec.Clock.ElapsedTicks < due) Thread.SpinWait(50);
                    midiOutShortMsg(h, e.Value);
                }
                long endAt = t0 + (long)((length + TailSeconds) * Stopwatch.Frequency);
                while (rec.Clock.ElapsedTicks < endAt) Thread.Sleep(5);
                midiOutReset(h);
                rec.Stop();
                rate = rec.Rate;

                // 録音の中での t0 の位置：時計は録音の直前に始めたので、ずれは録音の最初のパケットから測った t0。
                long clockAtCapture = rec.StartTicks - (Stopwatch.GetTimestamp() - rec.Clock.ElapsedTicks);
                int start = (int)Math.Max(0, (t0 - clockAtCapture) * (double)rate / Stopwatch.Frequency);
                int n = (int)Math.Round(length * rate), tail = (int)(TailSeconds * rate);
                var loop = new float[n];
                for (int i = 0; i < n; i++)
                {
                    int k = start + i;
                    loop[i] = k < rec.Mono.Count ? rec.Mono[k] : 0;
                    if (i < tail && k + n < rec.Mono.Count) loop[i] += rec.Mono[k + n];   // 尾を先頭に折り込む
                }
                return loop;
            }
            finally
            {
                timeEndPeriod(1);
                midiOutClose(h);
            }
        }

        // DS の IMA-ADPCM：ヘッダー（最初のサンプル、ステップの番号）、それからニブル。予測はデコーダーに
        // ぴったり従うので、誤差が積み重ならない。
        public static byte[] EncodeImaAdpcm(short[] s)
        {
            int count = s.Length & ~7;   // ニブルのワード単位
            var o = new byte[4 + count / 2];
            int pred = s.Length > 0 ? s[0] : 0, index = 0;
            o[0] = (byte)pred; o[1] = (byte)(pred >> 8); o[2] = 0; o[3] = 0;
            for (int i = 0; i < count; i++)
            {
                int step = ImaStep[index], diff = s[i] - pred, nib = 0;
                if (diff < 0) { nib = 8; diff = -diff; }
                int delta = step >> 3;
                if (diff >= step) { nib |= 4; diff -= step; delta += step; }
                if (diff >= step >> 1) { nib |= 2; diff -= step >> 1; delta += step >> 1; }
                if (diff >= step >> 2) { nib |= 1; delta += step >> 2; }
                pred += (nib & 8) != 0 ? -delta : delta;
                pred = Math.Max(-32767, Math.Min(32767, pred));   // DS は -0x7FFF..0x7FFF に制限する
                index = Math.Max(0, Math.Min(88, index + ImaIndex[nib]));
                o[4 + i / 2] |= (byte)(i % 2 == 0 ? nib : nib << 4);
            }
            return o;
        }

        // musicSpecs：「name=archive/folder:a,b」（.mid を除いた MIDI の名前）-> <outDir>/<name>.bin。
        // 録音は <wavDir>/<track>.wav にも出す。
        public static List<string> ExportMusic(string input, string outDir, string wavDir, string[] musicSpecs, int dsRate, Action<string> log)
        {
            Directory.CreateDirectory(outDir);
            Directory.CreateDirectory(wavDir);
            var files = new Dictionary<string, byte[]>(StringComparer.OrdinalIgnoreCase);
            foreach (var a in LoadArchives(input))
                foreach (var e in Hiz.Read(a.Value))
                {
                    if (!Hiz.IsSafeName(e.Name) || !e.Name.EndsWith(".mid", StringComparison.OrdinalIgnoreCase)) continue;
                    var data = new byte[e.Size];
                    Buffer.BlockCopy(a.Value, e.Offset, data, 0, e.Size);
                    files[a.Key + "/" + e.Name] = data;
                }
            var report = new List<string>();
            foreach (var spec in musicSpecs)
            {
                int eq = spec.IndexOf('='), colon = spec.IndexOf(':');
                if (eq <= 0 || colon < eq) throw new ArgumentException("music spec must be name=archive/folder:a,b: " + spec);
                string name = spec.Substring(0, eq), folder = spec.Substring(eq + 1, colon - eq - 1);
                var names = spec.Substring(colon + 1).Split(',');
                var tracks = new List<byte[]>();
                foreach (var n in names)
                {
                    byte[] mid;
                    if (!files.TryGetValue(folder + "/" + n + ".mid", out mid)) throw new FileNotFoundException("music not found: " + folder + "/" + n + ".mid");
                    int rate; double length;
                    log(string.Format("  {0}: recording {1} ...", name, n));
                    var loop = RenderMidi(mid, out rate, out length);
                    float peak = 1e-6f;
                    foreach (var v in loop) peak = Math.Max(peak, Math.Abs(v));
                    float gain = 0.9f / peak;
                    // DS のレートに再サンプリング（線形。シンセはそのあたりの成分が少ない）。
                    int m = (int)((long)loop.Length * dsRate / rate);
                    var pcm = new short[m];
                    for (int i = 0; i < m; i++)
                    {
                        double x = (double)i * rate / dsRate;
                        int k = (int)x; double f = x - k;
                        float a = loop[k % loop.Length], b2 = loop[(k + 1) % loop.Length];
                        pcm[i] = (short)Math.Max(-32767, Math.Min(32767, (a + (b2 - a) * f) * gain * 32767));
                    }
                    WritePcmWav(Path.Combine(wavDir, n + ".wav"), pcm, dsRate);
                    tracks.Add(EncodeImaAdpcm(pcm));
                    log(string.Format("  {0}: {1} {2:F1} s loop, peak {3:F2} before normalising", name, n, length, peak));
                    if (peak < 0.01f) log("  WARNING: almost silent: is the audio output muted, or the MIDI synth missing?");
                }
                long total = 0;
                using (var bw = new BinaryWriter(File.Create(Path.Combine(outDir, name + ".bin"))))
                {
                    bw.Write(Encoding.ASCII.GetBytes("RMU1"));
                    bw.Write((ushort)tracks.Count); bw.Write((ushort)dsRate);
                    foreach (var t in tracks) { bw.Write((uint)t.Length); bw.Write(t); total += t.Length; }
                }
                report.Add(string.Format("{0,-10} {1} tracks, {2:F1} KB", name, tracks.Count, total / 1024.0));
            }
            return report;
        }

        static void WritePcmWav(string path, short[] s, int rate)
        {
            using (var f = new BinaryWriter(File.Create(path)))
            {
                f.Write(Encoding.ASCII.GetBytes("RIFF")); f.Write(36 + s.Length * 2);
                f.Write(Encoding.ASCII.GetBytes("WAVEfmt ")); f.Write(16); f.Write((short)1); f.Write((short)1);
                f.Write(rate); f.Write(rate * 2); f.Write((short)2); f.Write((short)16);
                f.Write(Encoding.ASCII.GetBytes("data")); f.Write(s.Length * 2);
                foreach (var v in s) f.Write(v);
            }
        }
    }
}
