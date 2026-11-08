// Sound effects for the DS: IMA-ADPCM WAVs from the game -> one bank of 8-bit PCM samples.
//
// Bank (.bin, "RSN1"):
//   char magic[4] "RSN1"; u16 count, u16 0
//   per sound: u16 rate (Hz), u16 0, u32 bytes (a multiple of 4); s8 samples[bytes]
// Sounds keep the order of the spec; each is peak-normalised so they play at similar levels.
using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace Re4Zeebo
{
    public static partial class Tool
    {
        static readonly int[] ImaStep = {
            7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97,
            107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
            876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428,
            4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350,
            22385, 24623, 27086, 29794, 32767 };
        static readonly int[] ImaIndex = { -1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8 };

        // Mono WAV (PCM 16-bit or IMA-ADPCM) -> samples, rate.
        public static short[] DecodeWav(byte[] b, out int rate)
        {
            if (b.Length < 12 || Encoding.ASCII.GetString(b, 0, 4) != "RIFF" || Encoding.ASCII.GetString(b, 8, 4) != "WAVE")
                throw new InvalidDataException("not a WAV file");
            int fmt = 0, channels = 0, align = 0, bits = 0, perBlock = 0, dataOff = -1, dataLen = 0;
            rate = 0;
            for (int p = 12; p + 8 <= b.Length;)
            {
                string id = Encoding.ASCII.GetString(b, p, 4);
                int len = BitConverter.ToInt32(b, p + 4);
                if (id == "fmt ")
                {
                    fmt = BitConverter.ToUInt16(b, p + 8); channels = BitConverter.ToUInt16(b, p + 10);
                    rate = BitConverter.ToInt32(b, p + 12); align = BitConverter.ToUInt16(b, p + 20);
                    bits = BitConverter.ToUInt16(b, p + 22);
                    if (len >= 20) perBlock = BitConverter.ToUInt16(b, p + 26);
                }
                else if (id == "data") { dataOff = p + 8; dataLen = Math.Min(len, b.Length - dataOff); }
                p += 8 + len + (len & 1);
            }
            if (dataOff < 0 || channels != 1) throw new InvalidDataException("need a mono WAV with data");
            var o = new List<short>();
            if (fmt == 1 && bits == 16)
                for (int i = 0; i + 1 < dataLen; i += 2) o.Add(BitConverter.ToInt16(b, dataOff + i));
            else if (fmt == 0x11)
            {
                // Blocks: s16 predictor, u8 step index, u8 0, then nibbles (low first); the
                // predictor is the block's first sample.
                for (int blk = dataOff; blk + 4 <= dataOff + dataLen; blk += align)
                {
                    int pred = BitConverter.ToInt16(b, blk), index = Math.Min(88, (int)b[blk + 2]);
                    o.Add((short)pred);
                    int end = Math.Min(blk + align, dataOff + dataLen), n = 1;
                    for (int q = blk + 4; q < end && (perBlock == 0 || n < perBlock); q++)
                        for (int half = 0; half < 2 && (perBlock == 0 || n < perBlock); half++, n++)
                        {
                            int nib = half == 0 ? b[q] & 15 : b[q] >> 4, step = ImaStep[index];
                            int diff = step >> 3;
                            if ((nib & 1) != 0) diff += step >> 2;
                            if ((nib & 2) != 0) diff += step >> 1;
                            if ((nib & 4) != 0) diff += step;
                            pred += (nib & 8) != 0 ? -diff : diff;
                            pred = Math.Max(-32768, Math.Min(32767, pred));
                            index = Math.Max(0, Math.Min(88, index + ImaIndex[nib]));
                            o.Add((short)pred);
                        }
                }
            }
            else throw new InvalidDataException("unsupported WAV format " + fmt + "/" + bits + " bit");
            return o.ToArray();
        }

        // soundSpecs: "name=archive/folder:a,b,c" (WAV names without .wav) -> <outDir>/<name>.bin.
        public static List<string> ExportSounds(string input, string outDir, string[] soundSpecs, Action<string> log)
        {
            Directory.CreateDirectory(outDir);
            var files = new Dictionary<string, byte[]>(StringComparer.OrdinalIgnoreCase);
            foreach (var a in LoadArchives(input))
                foreach (var e in Hiz.Read(a.Value))
                {
                    if (!Hiz.IsSafeName(e.Name) || !e.Name.EndsWith(".wav", StringComparison.OrdinalIgnoreCase)) continue;
                    var data = new byte[e.Size];
                    Buffer.BlockCopy(a.Value, e.Offset, data, 0, e.Size);
                    files[a.Key + "/" + e.Name] = data;
                }
            var report = new List<string>();
            foreach (var spec in soundSpecs)
            {
                int eq = spec.IndexOf('='), colon = spec.IndexOf(':');
                if (eq <= 0 || colon < eq) throw new ArgumentException("sound spec must be name=archive/folder:a,b,c: " + spec);
                string name = spec.Substring(0, eq), folder = spec.Substring(eq + 1, colon - eq - 1);
                var names = spec.Substring(colon + 1).Split(',');
                long total = 0;
                using (var bw = new BinaryWriter(File.Create(Path.Combine(outDir, name + ".bin"))))
                {
                    bw.Write(Encoding.ASCII.GetBytes("RSN1"));
                    bw.Write((ushort)names.Length); bw.Write((ushort)0);
                    foreach (var n in names)
                    {
                        byte[] wav;
                        if (!files.TryGetValue(folder + "/" + n + ".wav", out wav)) throw new FileNotFoundException("sound not found: " + folder + "/" + n + ".wav");
                        int rate;
                        var s = DecodeWav(wav, out rate);
                        int peak = 1;
                        foreach (var v in s) peak = Math.Max(peak, Math.Abs((int)v));
                        double gain = 127.0 * 0.95 / peak;   // to 8 bits, peak at 95%
                        int bytes = (s.Length + 3) & ~3;
                        bw.Write((ushort)rate); bw.Write((ushort)0); bw.Write((uint)bytes);
                        for (int i = 0; i < bytes; i++)
                            bw.Write((sbyte)(i < s.Length ? Math.Max(-127, Math.Min(127, (int)Math.Round(s[i] * gain))) : 0));
                        total += bytes;
                        log(string.Format("  {0}: {1} {2:F2} s, peak {3}", name, n, (double)s.Length / rate, peak));
                    }
                }
                report.Add(string.Format("{0,-10} {1} sounds, {2:F1} KB", name, names.Length, total / 1024.0));
            }
            return report;
        }
    }
}
