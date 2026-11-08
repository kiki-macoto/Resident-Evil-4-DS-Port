// RE4 Zeebo Edition asset tools: H2Z/HIZ unpacking, MCT (ATC) -> PNG, batch driver. Models are in Mcm.cs.
using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Text;

namespace Re4Zeebo
{
    public class HizEntry
    {
        public string Name;
        public int Offset;   // absolute offset of data in the .h2z file
        public int Size;
        public int Size2;
        public ushort Flags;
    }

    public static class Hiz
    {
        static readonly byte[] Key = Encoding.ASCII.GetBytes("bio4");

        // Entry headers are XORed with "bio4". The key position advances only
        // over header bytes (the stream of all entry headers), not over file data.
        static byte[] Dec(byte[] b, int o, int n, ref int keyPos)
        {
            var r = new byte[n];
            for (int i = 0; i < n; i++) r[i] = (byte)(b[o + i] ^ Key[keyPos++ & 3]);
            return r;
        }

        public static List<HizEntry> Read(byte[] b)
        {
            if (b.Length < 0x18 || Encoding.ASCII.GetString(b, 0, 3) != "H2Z" || Encoding.ASCII.GetString(b, 12, 3) != "HIZ")
                throw new InvalidDataException("not an H2Z/HIZ archive");
            int count = BitConverter.ToInt32(b, 0x10);
            var list = new List<HizEntry>();
            int p = 0x18, keyPos = 0;
            for (int i = 0; i < count; i++)
            {
                if (p + 12 > b.Length) throw new InvalidDataException(string.Format("truncated HIZ entry {0}", i));
                var h = Dec(b, p, 12, ref keyPos);
                var e = new HizEntry();
                e.Flags = BitConverter.ToUInt16(h, 0);
                e.Size = BitConverter.ToInt32(h, 2);
                e.Size2 = BitConverter.ToInt32(h, 6);
                int nameLen = BitConverter.ToUInt16(h, 10);
                if (nameLen <= 0 || nameLen > 255 || e.Size < 0 || p + 12 + nameLen + e.Size > b.Length)
                    throw new InvalidDataException(string.Format("bad HIZ entry {0} at 0x{1:x}", i, p));
                e.Name = Encoding.ASCII.GetString(Dec(b, p + 12, nameLen, ref keyPos)).TrimEnd('\0');
                e.Offset = p + 12 + nameLen;
                list.Add(e);
                p = e.Offset + e.Size;
            }
            return list;
        }

        // Builds an archive (as Read() reads it) from the files under a folder, names relative
        // to it with '/' (for -Unpack's output when the original archives are gone).
        public static byte[] Pack(string dir)
        {
            var ms = new MemoryStream();
            var files = Directory.GetFiles(dir, "*", SearchOption.AllDirectories);
            ms.Write(Encoding.ASCII.GetBytes("H2Z\0\0\0\0\0\0\0\0\0HIZ\0"), 0, 16);
            ms.Write(BitConverter.GetBytes(files.Length), 0, 4);
            ms.Write(new byte[4], 0, 4);
            int keyPos = 0;
            foreach (var f in files)
            {
                var data = File.ReadAllBytes(f);
                var name = Encoding.ASCII.GetBytes(f.Substring(dir.Length + 1).Replace('\\', '/'));
                var h = new byte[12];
                BitConverter.GetBytes(data.Length).CopyTo(h, 2);
                BitConverter.GetBytes(data.Length).CopyTo(h, 6);
                BitConverter.GetBytes((ushort)name.Length).CopyTo(h, 10);
                var enc = Dec(h, 0, 12, ref keyPos);
                ms.Write(enc, 0, 12);
                enc = Dec(name, 0, name.Length, ref keyPos);
                ms.Write(enc, 0, enc.Length);
                ms.Write(data, 0, data.Length);
            }
            return ms.ToArray();
        }

        // Entry names may contain folders ("snd/x.wav"); folder entries end in '/'.
        public static bool IsSafeName(string name)
        {
            return name.Length > 0 && !name.EndsWith("/") && !name.Contains("..") && !name.Contains(":")
                && !name.StartsWith("/") && !name.StartsWith("\\");
        }
    }

    // .mcts / .mcms / .mcas sets: u32 count, u32 offsets[count + 1] (relative to set start).
    public static class ChunkSet
    {
        public static List<int[]> Read(byte[] b)
        {
            var r = new List<int[]>();
            if (b.Length < 8) return r;
            int count = BitConverter.ToInt32(b, 0);
            if (count <= 0 || 4 + (count + 1) * 4 > b.Length) return r;
            for (int i = 0; i < count; i++)
            {
                int s = BitConverter.ToInt32(b, 4 + i * 4), e = BitConverter.ToInt32(b, 8 + i * 4);
                if (s < 0 || e < s || e > b.Length) return new List<int[]>();
                r.Add(new[] { s, e - s });
            }
            return r;
        }
    }

    public class MctInfo
    {
        public int Width, Height, Format, Id, DataOffset, DataSize;
        public string FormatName
        {
            get
            {
                switch (Format)
                {
                    case 0x06: return "PAL8_RGB888";
                    case 0x15: return "ATC_RGB";
                    case 0x16: return "ATC_RGBA_EXPLICIT";
                    default: return "unknown_0x" + Format.ToString("x2");
                }
            }
        }
    }

    public static class Mct
    {
        public static bool IsMct(byte[] b, int o) { return o + 0x2A <= b.Length && b[o] == 'M' && b[o + 1] == 'C' && b[o + 2] == 'T' && b[o + 3] == '_'; }

        // Header (offsets from "MCT_"): +0x0A u32 bytes from +0x0A to end of data,
        // +0x1A u16 id, +0x22 u16 width, +0x24 u16 height, +0x28 u16 format.
        // Formats 0x15/0x16 have a 0x3E-byte header; 0x06 has 0x2A + a 256 x RGB888 palette.
        public static MctInfo Parse(byte[] b, int o)
        {
            if (!IsMct(b, o)) throw new InvalidDataException("no MCT_ magic");
            var m = new MctInfo();
            m.Id = BitConverter.ToUInt16(b, o + 0x1A);
            m.Width = BitConverter.ToUInt16(b, o + 0x22);
            m.Height = BitConverter.ToUInt16(b, o + 0x24);
            m.Format = BitConverter.ToUInt16(b, o + 0x28);
            int end = o + 0x0A + BitConverter.ToInt32(b, o + 0x0A);
            m.DataOffset = o + (m.Format == 0x06 ? 0x2A : 0x3E);
            m.DataSize = end - m.DataOffset;
            return m;
        }

        public static int ExpectedSize(MctInfo m)
        {
            switch (m.Format)
            {
                case 0x06: return 768 + m.Width * m.Height;
                case 0x15: return m.Width * m.Height / 2;
                case 0x16: return m.Width * m.Height;
                default: return -1;
            }
        }

        static void Rgb555(int c, int[] o) { o[0] = Ex5((c >> 10) & 31); o[1] = Ex5((c >> 5) & 31); o[2] = Ex5(c & 31); }
        static void Rgb565(int c, int[] o) { o[0] = Ex5((c >> 11) & 31); o[1] = ((c >> 5) & 63) << 2 | ((c >> 5) & 63) >> 4; o[2] = Ex5(c & 31); }
        static int Ex5(int v) { return v << 3 | v >> 2; }

        // ATC RGB block: c0 (RGB555 + mode bit 15), c1 (RGB565), 2-bit indices.
        static void AtcColorBlock(byte[] b, int p, byte[] rgba, int w, int bx, int by)
        {
            int c0 = BitConverter.ToUInt16(b, p), c1 = BitConverter.ToUInt16(b, p + 2);
            uint idx = BitConverter.ToUInt32(b, p + 4);
            int[] a = new int[3], z = new int[3];
            Rgb555(c0, a); Rgb565(c1, z);
            var pal = new int[4, 3];
            for (int k = 0; k < 3; k++)
            {
                if ((c0 & 0x8000) == 0)
                {
                    pal[0, k] = a[k]; pal[3, k] = z[k];
                    pal[1, k] = (5 * a[k] + 3 * z[k]) / 8;
                    pal[2, k] = (3 * a[k] + 5 * z[k]) / 8;
                }
                else
                {
                    pal[0, k] = 0; pal[2, k] = a[k]; pal[3, k] = z[k];
                    pal[1, k] = Math.Max(0, a[k] - z[k] / 4);
                }
            }
            for (int t = 0; t < 16; t++)
            {
                int s = (int)(idx >> (2 * t)) & 3;
                int px = (by * 4 + t / 4) * w + bx * 4 + t % 4;
                rgba[px * 4] = (byte)pal[s, 0]; rgba[px * 4 + 1] = (byte)pal[s, 1]; rgba[px * 4 + 2] = (byte)pal[s, 2];
            }
        }

        public static byte[] DecodeRgba(byte[] b, MctInfo m)
        {
            int w = m.Width, h = m.Height;
            var rgba = new byte[w * h * 4];
            int need = ExpectedSize(m);
            if (need < 0) throw new NotSupportedException("format 0x" + m.Format.ToString("x2"));
            if (m.DataSize < need || m.DataOffset + need > b.Length) throw new InvalidDataException("texture data truncated");
            int p = m.DataOffset;
            if (m.Format == 0x06)
            {
                int pix = p + 768;
                for (int i = 0; i < w * h; i++)
                {
                    int c = b[pix + i];
                    rgba[i * 4] = b[p + c * 3]; rgba[i * 4 + 1] = b[p + c * 3 + 1]; rgba[i * 4 + 2] = b[p + c * 3 + 2]; rgba[i * 4 + 3] = 255;
                }
                return rgba;
            }
            bool alpha = m.Format == 0x16;
            for (int by = 0; by < h / 4; by++)
                for (int bx = 0; bx < w / 4; bx++)
                {
                    ulong ab = 0;
                    if (alpha) { ab = BitConverter.ToUInt64(b, p); p += 8; }
                    AtcColorBlock(b, p, rgba, w, bx, by);
                    p += 8;
                    for (int t = 0; t < 16; t++)
                    {
                        int px = (by * 4 + t / 4) * w + bx * 4 + t % 4;
                        rgba[px * 4 + 3] = alpha ? (byte)(((int)(ab >> (4 * t)) & 15) * 17) : (byte)255;
                    }
                }
            return rgba;
        }
    }

    public static class Png
    {
        static uint[] crcTable;

        static uint Crc(byte[] d, int o, int n, uint c = 0xFFFFFFFF)
        {
            if (crcTable == null)
            {
                crcTable = new uint[256];
                for (uint i = 0; i < 256; i++) { uint x = i; for (int k = 0; k < 8; k++) x = (x & 1) != 0 ? 0xEDB88320 ^ (x >> 1) : x >> 1; crcTable[i] = x; }
            }
            for (int i = 0; i < n; i++) c = crcTable[(c ^ d[o + i]) & 0xFF] ^ (c >> 8);
            return c;
        }

        static void Chunk(Stream s, string type, byte[] data)
        {
            var t = Encoding.ASCII.GetBytes(type);
            var len = BitConverter.GetBytes(data.Length); Array.Reverse(len);
            s.Write(len, 0, 4); s.Write(t, 0, 4); s.Write(data, 0, data.Length);
            var crc = BitConverter.GetBytes(Crc(data, 0, data.Length, Crc(t, 0, 4)) ^ 0xFFFFFFFF); Array.Reverse(crc);
            s.Write(crc, 0, 4);
        }

        public static void Write(string path, int w, int h, byte[] rgba)
        {
            using (var fs = File.Create(path))
            {
                fs.Write(new byte[] { 137, 80, 78, 71, 13, 10, 26, 10 }, 0, 8);
                var ihdr = new byte[13];
                ihdr[0] = (byte)(w >> 24); ihdr[1] = (byte)(w >> 16); ihdr[2] = (byte)(w >> 8); ihdr[3] = (byte)w;
                ihdr[4] = (byte)(h >> 24); ihdr[5] = (byte)(h >> 16); ihdr[6] = (byte)(h >> 8); ihdr[7] = (byte)h;
                ihdr[8] = 8; ihdr[9] = 6;
                Chunk(fs, "IHDR", ihdr);
                var raw = new byte[(w * 4 + 1) * h];
                for (int y = 0; y < h; y++) Buffer.BlockCopy(rgba, y * w * 4, raw, y * (w * 4 + 1) + 1, w * 4);
                var ms = new MemoryStream();
                using (var z = new ZLibStream(ms, CompressionLevel.Optimal, true)) z.Write(raw, 0, raw.Length);
                Chunk(fs, "IDAT", ms.ToArray());
                Chunk(fs, "IEND", new byte[0]);
            }
        }
    }

    // Batch driver: reads a zip, a folder or a single .h2z and writes PNGs, OBJ models,
    // unpacked files and CSV reports.
    public static partial class Tool
    {
        class ArchiveTotals { public int Textures, TexBytes, DsBytes, Meshes, Triangles, Draws; }

        class TexRef { public string Png; public bool Alpha, Ok; public MctInfo Info; public byte[] Src; }

        // Texture id 1000+ is an MCT id (stage sets); a small id indexes the model's own
        // .mcts ("ch_leon.mcms" -> "ch_leon.mcts" or "ch_gna_all.mcts"), in its archive or 3d_base.
        static TexRef ResolveTexture(Dictionary<string, Dictionary<int, TexRef>> byId, Dictionary<string, List<TexRef>> byFile,
                                     string arch, string file, int id)
        {
            TexRef r;
            foreach (var ak in new[] { arch, "3d_base" })
                if (id != 0 && byId.ContainsKey(ak) && byId[ak].TryGetValue(id, out r)) return r;
            string bn = Path.GetFileNameWithoutExtension(file);
            foreach (var ak in new[] { arch, "3d_base" })
                foreach (var cand in new[] { bn + ".mcts", bn + "_all.mcts", bn + "_0.mcts" })
                {
                    List<TexRef> l;
                    if (byFile.TryGetValue(ak + "/" + cand, out l) && id < l.Count && l[id] != null && l[id].Ok) return l[id];
                }
            return null;
        }

        static string Csv(string s) { return s.IndexOfAny(new[] { ',', '"' }) >= 0 ? "\"" + s.Replace("\"", "\"\"") + "\"" : s; }

        static List<KeyValuePair<string, byte[]>> LoadArchives(string input)
        {
            var r = new List<KeyValuePair<string, byte[]>>();
            if (Directory.Exists(input))
            {
                foreach (var f in Directory.GetFiles(input, "*.h2z", SearchOption.AllDirectories))
                    r.Add(new KeyValuePair<string, byte[]>(Path.GetFileNameWithoutExtension(f), File.ReadAllBytes(f)));
                // No archives: an unpacked tree (-Unpack's out/files, one folder per archive),
                // packed back into archives in memory.
                if (r.Count == 0)
                    foreach (var d in Directory.GetDirectories(input))
                        r.Add(new KeyValuePair<string, byte[]>(Path.GetFileName(d), Hiz.Pack(d)));
            }
            else if (input.EndsWith(".zip", StringComparison.OrdinalIgnoreCase))
            {
                using (var z = new ZipArchive(File.OpenRead(input), ZipArchiveMode.Read))
                    foreach (var e in z.Entries)
                    {
                        if (!e.FullName.EndsWith(".h2z", StringComparison.OrdinalIgnoreCase)) continue;
                        var ms = new MemoryStream();
                        using (var s = e.Open()) s.CopyTo(ms);
                        r.Add(new KeyValuePair<string, byte[]>(Path.GetFileNameWithoutExtension(e.Name), ms.ToArray()));
                    }
            }
            else r.Add(new KeyValuePair<string, byte[]>(Path.GetFileNameWithoutExtension(input), File.ReadAllBytes(input)));
            r.Sort((a, b) => string.CompareOrdinal(a.Key, b.Key));
            return r;
        }

        static List<int[]> Parts(string ext, byte[] data, string single)
        {
            return ext == single ? new List<int[]> { new[] { 0, data.Length } } : ChunkSet.Read(data);
        }

        public static List<string> Run(string input, string outDir, bool unpack, bool preview, Action<string> log)
        {
            var archives = LoadArchives(input);
            if (archives.Count == 0) throw new FileNotFoundException("no .h2z archives found in " + input);
            Directory.CreateDirectory(outDir);
            var tex = new StringBuilder("archive,file,index,id,width,height,format,bytes,ds_tex4x4_bytes,png\n");
            var mesh = new StringBuilder("archive,file,part,draws,triangles,indices,vertices,skinned,unmapped_draws,obj\n");
            var draws = new StringBuilder("archive,file,part,offset,prim,indices,triangles,max_index,material,texture_id,texture\n");
            var totals = new SortedDictionary<string, ArchiveTotals>(StringComparer.Ordinal);

            // Pass 1: unpack and convert every texture, so models in any archive can find them.
            var byId = new Dictionary<string, Dictionary<int, TexRef>>();          // archive -> MCT id -> texture
            var byFile = new Dictionary<string, List<TexRef>>();                    // "archive/file.mcts" -> parts
            var models = new List<Tuple<string, string, byte[]>>();
            foreach (var a in archives)
            {
                var t = totals[a.Key] = new ArchiveTotals();
                byId[a.Key] = new Dictionary<int, TexRef>();
                var entries = Hiz.Read(a.Value);
                log(string.Format("{0}: {1} files", a.Key, entries.Count));
                foreach (var e in entries)
                {
                    if (!Hiz.IsSafeName(e.Name)) continue;
                    var data = new byte[e.Size];
                    Buffer.BlockCopy(a.Value, e.Offset, data, 0, e.Size);
                    if (unpack)
                    {
                        var p = Path.Combine(outDir, "files", a.Key, e.Name);
                        Directory.CreateDirectory(Path.GetDirectoryName(p));
                        File.WriteAllBytes(p, data);
                    }
                    string ext = Path.GetExtension(e.Name).ToLowerInvariant();
                    string baseName = Path.GetFileNameWithoutExtension(e.Name);
                    if (ext == ".mcm" || ext == ".mcms") { models.Add(Tuple.Create(a.Key, e.Name, data)); continue; }
                    if (ext != ".mct" && ext != ".mcts") continue;

                    var parts = Parts(ext, data, ".mct");
                    var list = byFile[a.Key + "/" + e.Name] = new List<TexRef>();
                    for (int i = 0; i < parts.Count; i++)
                    {
                        int o = parts[i][0];
                        if (!Mct.IsMct(data, o)) { log("  skip (no MCT_): " + e.Name + "#" + i); list.Add(null); continue; }
                        var m = Mct.Parse(data, o);
                        var r = new TexRef { Info = m, Src = data, Alpha = m.Format == 0x16 };
                        int ds = m.Width * m.Height * 3 / 8;
                        try
                        {
                            var rgba = Mct.DecodeRgba(data, m);
                            string rel = Path.Combine("textures", a.Key, parts.Count > 1 ? string.Format("{0}_{1:D2}.png", baseName, i) : baseName + ".png");
                            Directory.CreateDirectory(Path.GetDirectoryName(Path.Combine(outDir, rel)));
                            Png.Write(Path.Combine(outDir, rel), m.Width, m.Height, rgba);
                            r.Png = rel.Replace('\\', '/');
                            r.Ok = true;
                        }
                        catch (Exception ex) { log("  texture " + e.Name + "#" + i + ": " + ex.Message); }
                        list.Add(r);
                        if (m.Id != 0 && r.Ok) byId[a.Key][m.Id] = r;
                        tex.AppendFormat("{0},{1},{2},{3},{4},{5},{6},{7},{8},{9}\n", a.Key, Csv(e.Name), i, m.Id, m.Width, m.Height, m.FormatName, m.DataSize, ds, Csv(r.Png ?? ""));
                        t.Textures++; t.TexBytes += m.DataSize; t.DsBytes += ds;
                    }
                }
            }

            Func<string, string, int, TexRef> resolve = (arch, file, id) => ResolveTexture(byId, byFile, arch, file, id);
            var rgbaCache = new Dictionary<TexRef, Preview.Tex>();

            // Pass 2: models.
            foreach (var mdl in models)
            {
                string arch = mdl.Item1, file = mdl.Item2;
                var data = mdl.Item3;
                string ext = Path.GetExtension(file).ToLowerInvariant();
                var parts = Parts(ext, data, ".mcm");
                for (int i = 0; i < parts.Count; i++)
                {
                    var s = Mcm.Parse(data, parts[i][0], parts[i][1], file);
                    Func<int, TexRef> texOf = key =>
                    {
                        var mat = s.Materials.Find(x => x.Key == key);
                        return mat == null ? null : resolve(arch, file, mat.TextureId);
                    };
                    string objRel = "";
                    bool skinned = s.VertexBlocks.Exists(v => v.Skinned);
                    if (s.Draws.Exists(d => d.Vertices != null))
                    {
                        string name = Path.GetFileNameWithoutExtension(file) + (parts.Count > 1 ? "_" + i.ToString("D2") : "");
                        objRel = Path.Combine("models", arch, name + ".obj");
                        string objPath = Path.Combine(outDir, objRel);
                        Directory.CreateDirectory(Path.GetDirectoryName(objPath));
                        ObjExport.Write(objPath, s, key =>
                        {
                            var r = texOf(key);
                            return r == null ? null : Tuple.Create("../../" + r.Png, r.Alpha);
                        });
                        if (preview)
                        {
                            Preview.Render(Path.ChangeExtension(objPath, ".preview.png"), s, key =>
                            {
                                var r = texOf(key);
                                if (r == null) return null;
                                Preview.Tex pt;
                                if (!rgbaCache.TryGetValue(r, out pt))
                                    rgbaCache[r] = pt = new Preview.Tex { W = r.Info.Width, H = r.Info.Height, Rgba = Mct.DecodeRgba(r.Src, r.Info) };
                                return pt;
                            }, 512);
                        }
                        objRel = objRel.Replace('\\', '/');
                    }
                    mesh.AppendFormat("{0},{1},{2},{3},{4},{5},{6},{7},{8},{9}\n", arch, Csv(file), i, s.DrawCalls, s.Triangles, s.Indices, s.Vertices, skinned ? 1 : 0, s.UnmappedDraws, Csv(objRel));
                    foreach (var d in s.Draws)
                    {
                        var mat = s.Materials.Find(x => x.Key == d.Material);
                        var r = texOf(d.Material);
                        draws.AppendFormat("{0},{1},{2},0x{3:x},{4},{5},{6},{7},{8},{9},{10}\n", arch, Csv(file), i, d.Offset, d.PrimType == 5 ? "strip" : "list", d.IndexCount, d.TriCount, d.MaxIndex, d.Material, mat == null ? "" : mat.TextureId.ToString(), Csv(r == null ? "" : r.Png));
                    }
                    var t = totals[arch];
                    t.Meshes++; t.Triangles += s.Triangles; t.Draws += s.DrawCalls;
                }
            }

            File.WriteAllText(Path.Combine(outDir, "textures.csv"), tex.ToString());
            File.WriteAllText(Path.Combine(outDir, "meshes.csv"), mesh.ToString());
            File.WriteAllText(Path.Combine(outDir, "draws.csv"), draws.ToString());

            var summary = new List<string>();
            summary.Add(string.Format("{0,-14} {1,5} {2,9} {3,9} {4,6} {5,9} {6,6}", "archive", "tex", "texKB", "ds4x4KB", "meshes", "triangles", "draws"));
            foreach (var kv in totals)
            {
                var t = kv.Value;
                summary.Add(string.Format("{0,-14} {1,5} {2,9:F1} {3,9:F1} {4,6} {5,9} {6,6}", kv.Key, t.Textures, t.TexBytes / 1024.0, t.DsBytes / 1024.0, t.Meshes, t.Triangles, t.Draws));
            }
            File.WriteAllLines(Path.Combine(outDir, "summary.txt"), summary);
            return summary;
        }
    }
}
