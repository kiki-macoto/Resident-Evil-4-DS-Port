// 2D exports for the DS: picture sheets (HUD parts, title art) and the game font.
//
// Sheet (.bin, "RD2D"): a part of an MCT texture (or of texture #part of an .mcts set),
// optionally resized, as one DS texture: 256 colours, or with "!glow" A5I3 with the brightness
// as alpha (for light effects that the original adds to the picture).
// With "!bmp" a plain 2D bitmap instead ("RD2B": u16 width, height, then RGB15 pixels with
// bit 15 set where opaque), for the 16-bit background of the bottom screen.
//   char magic[4] "RD2D"; u16 imageWidth, imageHeight (the picture; the texture is padded
//   to powers of two from the top left); then one texture record as in RDS1 models:
//   u16 width, height; u8 type (4 = GL_RGB256); u8 flags (1 = colour 0 transparent);
//   u16 paletteColors; u32 dataBytes; u8 data[]; u16 palette[]; pad to 4.
//
// Font (.bin, "RDF1"): the glyphs of 2d_fnt_00.mct that a text file uses (plus printable ASCII).
//   char magic[4] "RDF1"; u16 glyphCount, u16 height (14)
//   glyphs sorted by code: u16 code (Unicode), u8 width (7 or 14), u8 0, u32 rows[height]
//   Each row holds 2 bits per pixel, leftmost pixel in the low bits: 0 = empty, 1..3 = dark..light.
//
// The font texture is a grid of 14-pixel rows: first the single-byte characters of 2d_fnt.hfi
// in 7-pixel cells, then the double-byte (Shift-JIS) ones in 14-pixel cells, filling each row
// left to right.
using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace Re4Zeebo
{
    public static partial class Tool
    {
        // sheetSpecs: "name=archive/file.mct@x,y,w,h[>W,H]" (crop, then resize to W x H).
        // fontSpecs:  "name=archive/font.mct;archive/font.hfi;textfile" (the text file lists the
        //             characters to keep, e.g. the C source with the menu strings).
        public static List<string> Export2D(string input, string outDir, string[] sheetSpecs, string[] fontSpecs, Action<string> log)
        {
            Directory.CreateDirectory(outDir);
            var files = new Dictionary<string, byte[]>(StringComparer.OrdinalIgnoreCase);
            foreach (var a in LoadArchives(input))
                foreach (var e in Hiz.Read(a.Value))
                {
                    if (!Hiz.IsSafeName(e.Name)) continue;
                    var data = new byte[e.Size];
                    Buffer.BlockCopy(a.Value, e.Offset, data, 0, e.Size);
                    files[a.Key + "/" + e.Name] = data;
                }
            Func<string, byte[]> file = p =>
            {
                byte[] d;
                if (!files.TryGetValue(p, out d)) throw new FileNotFoundException("not found: " + p);
                return d;
            };

            var report = new List<string>();
            foreach (var spec in sheetSpecs ?? new string[0])
            {
                string sp = spec;
                bool glow = sp.EndsWith("!glow");
                if (glow) sp = sp.Substring(0, sp.Length - 5);
                bool bmp = sp.EndsWith("!bmp");
                if (bmp) sp = sp.Substring(0, sp.Length - 4);
                int eq = sp.IndexOf('=');
                if (eq <= 0) throw new ArgumentException("sheet spec must be name=archive/file.mct[#part]@x,y,w,h[>W,H][!cutWxH][|...][!glow]: " + spec);
                string name = sp.Substring(0, eq);
                // Several sources separated by '|' are stacked top to bottom, left-aligned.
                var pieces = new List<Tuple<byte[], int, int>>();
                foreach (var src in sp.Substring(eq + 1).Split('|'))
                {
                    int pw, ph;
                    var px = LoadPiece(src, file, out pw, out ph);
                    pieces.Add(Tuple.Create(px, pw, ph));
                }
                int iw = 0, ih = 0;
                foreach (var p in pieces) { iw = Math.Max(iw, p.Item2); ih += p.Item3; }
                var rgba = new byte[iw * ih * 4];
                int y0 = 0;
                foreach (var p in pieces)
                {
                    for (int y = 0; y < p.Item3; y++)
                        Buffer.BlockCopy(p.Item1, y * p.Item2 * 4, rgba, ((y0 + y) * iw) * 4, p.Item2 * 4);
                    if (pieces.Count > 1) log(string.Format("  {0}: piece at y {1} ({2}x{3})", name, y0, p.Item2, p.Item3));
                    y0 += p.Item3;
                }
                if (bmp)
                {
                    // 2D bitmap for a 16-bit background (the bottom screen): RGB15, bit 15 = opaque.
                    using (var bw = new BinaryWriter(File.Create(Path.Combine(outDir, name + ".bin"))))
                    {
                        bw.Write(Encoding.ASCII.GetBytes("RD2B"));
                        bw.Write((ushort)iw); bw.Write((ushort)ih);
                        for (int i = 0; i < iw * ih; i++)
                            bw.Write((ushort)(rgba[i * 4 + 3] >= 128 ? 0x8000 | DsTextures.Rgb15(rgba[i * 4], rgba[i * 4 + 1], rgba[i * 4 + 2]) : 0));
                    }
                    report.Add(string.Format("{0,-10} {1}x{2} bitmap, {3:F1} KB", name, iw, ih, iw * ih * 2 / 1024.0));
                    continue;
                }
                int tw = Pow2(iw), th = Pow2(ih);
                var padded = Crop(rgba, iw, 0, 0, tw, th, ih);
                var tex = glow ? DsTextures.ToGlow(padded, tw, th) : DsTextures.ToRgb256(padded, tw, th);
                using (var bw = new BinaryWriter(File.Create(Path.Combine(outDir, name + ".bin"))))
                {
                    bw.Write(Encoding.ASCII.GetBytes("RD2D"));
                    bw.Write((ushort)iw); bw.Write((ushort)ih);
                    WriteTexture(bw, tex);
                }
                report.Add(string.Format("{0,-10} {1}x{2} picture in a {3}x{4} texture, {5:F1} KB", name, iw, ih, tw, th, tex.Bytes / 1024.0));
            }

            foreach (var spec in fontSpecs ?? new string[0])
            {
                int eq = spec.IndexOf('=');
                var parts = eq > 0 ? spec.Substring(eq + 1).Split(';') : null;
                if (parts == null || parts.Length != 3) throw new ArgumentException("font spec must be name=font.mct;font.hfi;textfile: " + spec);
                string name = spec.Substring(0, eq);
                report.Add(WriteFont(Path.Combine(outDir, name + ".bin"), name, file(parts[0]), file(parts[1]), File.ReadAllText(parts[2], Encoding.UTF8), log));
            }
            return report;
        }

        // Reads an image file: returns { byte[] BGRA pixels, int width, int height } (set by
        // re4zeebo.ps1, which has System.Drawing at hand).
        public static Func<string, object[]> ImageLoader;

        // One source of a sheet: "archive/file.mct[#part]@x,y,w,h[>W,H][!cutWxH]", or
        // "file:path/image.png@..." for an image file.
        static byte[] LoadPiece(string src, Func<string, byte[]> file, out int iw, out int ih)
        {
            // "!cutWxH": icons on black in a grid of W x H cells; the black reaching each
            // cell's edge becomes transparent (black inside an icon stays).
            int cutW = 0, cutH = 0, cut = src.LastIndexOf("!cut");
            if (cut > 0)
            {
                var c = src.Substring(cut + 4).Split('x');
                cutW = int.Parse(c[0]); cutH = int.Parse(c[1]);
                src = src.Substring(0, cut);
            }
            int at = src.IndexOf('@');
            if (at <= 0) throw new ArgumentException("sheet source must be archive/file.mct[#part]@x,y,w,h: " + src);
            string path = src.Substring(0, at);
            string[] geo = src.Substring(at + 1).Split('>');
            var r = Array.ConvertAll(geo[0].Split(','), int.Parse);
            int w, h;
            byte[] rgba;
            if (path.StartsWith("file:"))
            {
                // An image file on disk (e.g. an edited picture), read by ImageLoader.
                if (ImageLoader == null) throw new InvalidOperationException("no image loader for " + path);
                var img = ImageLoader(path.Substring(5));
                var bgra = (byte[])img[0];
                w = (int)img[1]; h = (int)img[2];
                rgba = new byte[w * h * 4];
                for (int i = 0; i < w * h; i++)
                {
                    rgba[i * 4] = bgra[i * 4 + 2]; rgba[i * 4 + 1] = bgra[i * 4 + 1];
                    rgba[i * 4 + 2] = bgra[i * 4]; rgba[i * 4 + 3] = bgra[i * 4 + 3];
                }
            }
            else
            {
                int part = 0, hash = path.IndexOf('#');
                if (hash >= 0) { part = int.Parse(path.Substring(hash + 1)); path = path.Substring(0, hash); }
                rgba = LoadRgba(file(path), out w, out h, part, Path.GetExtension(path).ToLowerInvariant());
            }
            rgba = Crop(rgba, w, r[0], r[1], r[2], r[3]);
            iw = r[2]; ih = r[3];
            if (cutW > 0) CutBlack(rgba, iw, ih, cutW, cutH);
            if (geo.Length > 1)
            {
                var s = Array.ConvertAll(geo[1].Split(','), int.Parse);
                rgba = Resize(rgba, iw, ih, s[0], s[1]);
                iw = s[0]; ih = s[1];
            }
            return rgba;
        }

        static byte[] LoadRgba(byte[] data, out int w, out int h, int part = 0, string ext = ".mct")
        {
            var p = Parts(ext, data, ".mct");
            if (part >= p.Count) throw new ArgumentException("no texture " + part + " in the set");
            var m = Mct.Parse(data, p[part][0]);
            w = m.Width; h = m.Height;
            return Mct.DecodeRgba(data, m);
        }

        // Flood fill from every near-black pixel on a cell border, within the cell.
        static void CutBlack(byte[] rgba, int w, int h, int cw, int ch)
        {
            Func<int, bool> dark = i => rgba[i * 4 + 3] >= 128 && Math.Max(rgba[i * 4], Math.Max(rgba[i * 4 + 1], rgba[i * 4 + 2])) <= 12;
            var stack = new Stack<int>();
            for (int y = 0; y < h; y++)
                for (int x = 0; x < w; x++)
                    if ((x % cw == 0 || x % cw == cw - 1 || y % ch == 0 || y % ch == ch - 1) && dark(y * w + x))
                        stack.Push(y * w + x);
            while (stack.Count > 0)
            {
                int i = stack.Pop();
                if (rgba[i * 4 + 3] == 0 || !dark(i)) continue;
                rgba[i * 4 + 3] = 0;
                int x = i % w, y = i / w;
                if (x % cw > 0) stack.Push(i - 1);
                if (x % cw < cw - 1 && x + 1 < w) stack.Push(i + 1);
                if (y % ch > 0) stack.Push(i - w);
                if (y % ch < ch - 1 && y + 1 < h) stack.Push(i + w);
            }
        }

        static int Pow2(int v) { int p = 8; while (p < v) p <<= 1; return p; }

        // Copies the w x h block at (x, y); pixels outside the source (or below srcRows) are clear.
        static byte[] Crop(byte[] rgba, int srcW, int x, int y, int w, int h, int srcRows = int.MaxValue)
        {
            int srcH = Math.Min(srcRows, rgba.Length / 4 / srcW);
            var o = new byte[w * h * 4];
            for (int j = 0; j < h; j++)
                for (int i = 0; i < w; i++)
                {
                    int sx = x + i, sy = y + j;
                    if (sx < 0 || sy < 0 || sx >= srcW || sy >= srcH) continue;
                    Buffer.BlockCopy(rgba, (sy * srcW + sx) * 4, o, (j * w + i) * 4, 4);
                }
            return o;
        }

        // Area-average resize (each output pixel averages the source area it covers).
        static byte[] Resize(byte[] rgba, int w, int h, int nw, int nh)
        {
            var o = new byte[nw * nh * 4];
            for (int y = 0; y < nh; y++)
                for (int x = 0; x < nw; x++)
                {
                    double x0 = (double)x * w / nw, x1 = (double)(x + 1) * w / nw;
                    double y0 = (double)y * h / nh, y1 = (double)(y + 1) * h / nh;
                    var acc = new double[4]; double area = 0;
                    for (int sy = (int)y0; sy < Math.Min(h, (int)Math.Ceiling(y1)); sy++)
                        for (int sx = (int)x0; sx < Math.Min(w, (int)Math.Ceiling(x1)); sx++)
                        {
                            double a = (Math.Min(x1, sx + 1) - Math.Max(x0, sx)) * (Math.Min(y1, sy + 1) - Math.Max(y0, sy));
                            for (int c = 0; c < 4; c++) acc[c] += rgba[(sy * w + sx) * 4 + c] * a;
                            area += a;
                        }
                    for (int c = 0; c < 4; c++) o[(y * nw + x) * 4 + c] = (byte)Math.Round(acc[c] / area);
                }
            return o;
        }

        static void WriteTexture(BinaryWriter bw, DsTexture t)
        {
            bw.Write((ushort)t.W); bw.Write((ushort)t.H);
            bw.Write((byte)t.Type);
            bw.Write((byte)(t.Transparent ? 1 : 0));
            bw.Write((ushort)t.Palette.Length);
            bw.Write((uint)t.Indices.Length);
            bw.Write(t.Indices);
            foreach (var c in t.Palette) bw.Write(c);
            while (bw.BaseStream.Position % 4 != 0) bw.Write((byte)0);
        }

        const int FontCell = 14;

        static string WriteFont(string path, string name, byte[] mct, byte[] hfi, string text, Action<string> log)
        {
            if (hfi.Length < 11 || hfi[0] != 'H' || hfi[1] != 'F' || hfi[2] != 'I') throw new InvalidDataException("not an HFI font map");
            int singles = hfi[7] | hfi[8] << 8, doubles = hfi[9] | hfi[10] << 8;
            var sjis = Encoding.GetEncoding(932);
            // Glyph position by character: walk the grid as the game lays it out.
            var cells = new Dictionary<char, int[]>();   // x, y, width
            int cx = 0, cy = 0;
            for (int i = 0; i < singles + doubles; i++)
            {
                int cw = i < singles ? FontCell / 2 : FontCell;
                if (cx + cw > 512) { cx = 0; cy += FontCell; }
                string s;
                if (i < singles)
                    s = sjis.GetString(new[] { hfi[11 + i] });
                else
                {
                    int o = 11 + singles + (i - singles) * 2;
                    s = sjis.GetString(new[] { hfi[o + 1], hfi[o] });   // stored as little-endian u16
                }
                if (s.Length == 1 && !cells.ContainsKey(s[0])) cells[s[0]] = new[] { cx, cy, cw };
                cx += cw;
            }

            var want = new SortedSet<char>();
            for (char c = '!'; c <= '~'; c++) want.Add(c);
            foreach (char c in text) if (c > '~') want.Add(c);
            var missing = new StringBuilder();
            int w, h;
            byte[] rgba = LoadRgba(mct, out w, out h);
            var glyphs = new List<Tuple<char, int, uint[]>>();
            foreach (char c in want)
            {
                int[] cell;
                if (!cells.TryGetValue(c, out cell))
                {
                    if (!char.IsWhiteSpace(c) && !char.IsControl(c) && c != '﻿') missing.Append(c);
                    continue;
                }
                var rows = new uint[FontCell];
                for (int y = 0; y < FontCell; y++)
                    for (int x = 0; x < cell[2]; x++)
                    {
                        int p = ((cell[1] + y) * w + cell[0] + x) * 4;
                        if (rgba[p + 3] < 128) continue;
                        int l = (rgba[p] + rgba[p + 1] + rgba[p + 2]) / 3;
                        uint v = l < 85 ? 1u : l < 170 ? 2u : 3u;
                        rows[y] |= v << (2 * x);
                    }
                glyphs.Add(Tuple.Create(c, cell[2], rows));
            }
            using (var bw = new BinaryWriter(File.Create(path)))
            {
                bw.Write(Encoding.ASCII.GetBytes("RDF1"));
                bw.Write((ushort)glyphs.Count); bw.Write((ushort)FontCell);
                foreach (var g in glyphs)
                {
                    bw.Write((ushort)g.Item1); bw.Write((byte)g.Item2); bw.Write((byte)0);
                    foreach (var r in g.Item3) bw.Write(r);
                }
            }
            if (missing.Length > 0) log("  " + name + ": no glyph for: " + missing);
            return string.Format("{0,-10} {1} glyphs ({2:F1} KB)", name, glyphs.Count, glyphs.Count * (4 + 4 * FontCell) / 1024.0);
        }
    }
}
