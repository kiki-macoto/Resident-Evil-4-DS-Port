// Export MCM models + their MCT textures to a DS-ready binary (.bin, little endian):
//
//   header (64 bytes)
//     char  magic[4] "RDS1"
//     u16   textureCount, drawCount
//     s32   center[3]      f32 (20.12): model-space point the vertices are relative to
//     u16   shift          vertices are stored divided by 2^shift; draw with glScalef32(1 << shift)
//     u16   flags          1 = skinned: vertices are in their bone's bind space, lists switch
//                          bones with MTX_RESTORE(slot base + node), skeleton follows the draws
//     s32   bboxMin[3], bboxMax[3]   f32, model space
//     s32   ground[3]      f32, highest surface point under the bbox centre (spawn point)
//     u32   triangleCount
//   textures[textureCount]
//     u16 width, height; u8 type (GL_TEXTURE_TYPE_ENUM); u8 flags (1 = colour 0 transparent);
//     u16 paletteColors; u32 dataBytes; u8 data[dataBytes]; u16 palette[paletteColors]; pad to 4
//   draws[drawCount]
//     u16 texture (0xFFFF = none); u8 transparent; u8 pad; u32 triangles;
//     s32 centre[3], radius   f32, world space: chunked models only (flag 2: the stages)
//     u32 list[1 + n]      glCallList() packed GX list, list[0] = n
//   skeleton (skinned only)
//     u16 nodes, u16 slotBase; per node: s16 parent, u16 0, s32 local[12] (f32: 3x3 rotation rows, translation)
// Animations go to a separate RDA1 file (see WriteAnimations).
using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace Re4Zeebo
{
    public class DsTexture
    {
        public int W, H;
        public bool Transparent;
        public int Type = 4;   // GL_TEXTURE_TYPE_ENUM: 4 = GL_RGB256, 6 = GL_RGB8_A5 (A5I3)
        public byte[] Indices;
        public ushort[] Palette;
        public int Bytes { get { return Indices.Length + Palette.Length * 2; } }
    }

    public static class DsTextures
    {
        public static int Rgb15(int r, int g, int b) { return (r >> 3) | (g >> 3) << 5 | (b >> 3) << 10; }

        // Box filter by 2^shift in each direction (alpha averaged too).
        public static byte[] Downscale(byte[] rgba, int w, int h, int shift, out int nw, out int nh)
        {
            int f = 1 << shift;
            nw = Math.Max(8, w >> shift); nh = Math.Max(8, h >> shift);
            var o = new byte[nw * nh * 4];
            for (int y = 0; y < nh; y++)
                for (int x = 0; x < nw; x++)
                    for (int c = 0; c < 4; c++)
                    {
                        int sum = 0, n = 0;
                        for (int dy = 0; dy < f && y * f + dy < h; dy++)
                            for (int dx = 0; dx < f && x * f + dx < w; dx++) { sum += rgba[((y * f + dy) * w + x * f + dx) * 4 + c]; n++; }
                        o[(y * nw + x) * 4 + c] = (byte)(sum / Math.Max(1, n));
                    }
            return o;
        }

        // 256-colour (GL_RGB256) conversion with median cut in RGB555 space.
        // Pixels with alpha < 128 map to index 0, which is then transparent.
        public static DsTexture ToRgb256(byte[] rgba, int w, int h)
        {
            var hist = new Dictionary<int, int>();
            bool transparent = false;
            for (int i = 0; i < w * h; i++)
            {
                if (rgba[i * 4 + 3] < 128) { transparent = true; continue; }
                int c = Rgb15(rgba[i * 4], rgba[i * 4 + 1], rgba[i * 4 + 2]);
                int n; hist.TryGetValue(c, out n); hist[c] = n + 1;
            }
            int first = transparent ? 1 : 0;
            var pal = MedianCut(hist, 256 - first);
            var map = new Dictionary<int, int>();
            foreach (var c in hist.Keys) map[c] = Nearest(pal, c) + first;

            var t = new DsTexture { W = w, H = h, Transparent = transparent, Indices = new byte[w * h] };
            for (int i = 0; i < w * h; i++)
                t.Indices[i] = rgba[i * 4 + 3] < 128 ? (byte)0 : (byte)map[Rgb15(rgba[i * 4], rgba[i * 4 + 1], rgba[i * 4 + 2])];
            t.Palette = new ushort[pal.Count + first];
            for (int i = 0; i < pal.Count; i++) t.Palette[i + first] = (ushort)pal[i];
            return t;
        }

        // A5I3 (GL_RGB8_A5) for glows drawn over the scene: the source is light on black (the
        // original adds it to the picture), so brightness becomes the alpha (5 bits) and the
        // colour, brought back to full brightness, one of 8 palette entries.
        public static DsTexture ToGlow(byte[] rgba, int w, int h)
        {
            var hist = new Dictionary<int, int>();
            var alpha = new int[w * h];
            var col = new int[w * h];
            for (int i = 0; i < w * h; i++)
            {
                int r = rgba[i * 4], g = rgba[i * 4 + 1], b = rgba[i * 4 + 2];
                int m = Math.Max(r, Math.Max(g, b)) * rgba[i * 4 + 3] / 255;
                alpha[i] = (m * 31 + 127) / 255;
                if (alpha[i] == 0) continue;
                int top = Math.Max(1, Math.Max(r, Math.Max(g, b)));
                col[i] = Rgb15(r * 255 / top, g * 255 / top, b * 255 / top);
                int n; hist.TryGetValue(col[i], out n); hist[col[i]] = n + alpha[i];
            }
            var pal = hist.Count > 0 ? MedianCut(hist, 8) : new List<int> { 0x7FFF };
            var t = new DsTexture { W = w, H = h, Type = 6, Indices = new byte[w * h], Palette = new ushort[8] };
            for (int i = 0; i < pal.Count; i++) t.Palette[i] = (ushort)pal[i];
            for (int i = 0; i < w * h; i++)
                t.Indices[i] = (byte)(alpha[i] << 3 | (alpha[i] > 0 ? Nearest(pal, col[i]) : 0));
            return t;
        }

        static int Ch(int c, int k) { return (c >> (5 * k)) & 31; }

        static List<int> MedianCut(Dictionary<int, int> hist, int max)
        {
            var boxes = new List<List<int>> { new List<int>(hist.Keys) };
            if (hist.Count <= max) return new List<int>(hist.Keys);
            while (boxes.Count < max)
            {
                // Split the box with the largest channel range, weighted by pixel count.
                int best = -1, bestCh = 0; long bestScore = 0;
                for (int i = 0; i < boxes.Count; i++)
                {
                    var b = boxes[i];
                    if (b.Count < 2) continue;
                    long count = 0; foreach (var c in b) count += hist[c];
                    for (int k = 0; k < 3; k++)
                    {
                        int lo = 31, hi = 0;
                        foreach (var c in b) { lo = Math.Min(lo, Ch(c, k)); hi = Math.Max(hi, Ch(c, k)); }
                        long score = (long)(hi - lo) * count;
                        if (score > bestScore) { bestScore = score; best = i; bestCh = k; }
                    }
                }
                if (best < 0) break;
                var box = boxes[best];
                box.Sort((x, y) => Ch(x, bestCh).CompareTo(Ch(y, bestCh)));
                long total = 0; foreach (var c in box) total += hist[c];
                long acc = 0; int cut = 1;
                for (int i = 0; i < box.Count - 1; i++) { acc += hist[box[i]]; if (acc * 2 >= total) { cut = i + 1; break; } }
                boxes[best] = box.GetRange(0, cut);
                boxes.Add(box.GetRange(cut, box.Count - cut));
            }
            var pal = new List<int>();
            foreach (var b in boxes)
            {
                long r = 0, g = 0, bl = 0, n = 0;
                foreach (var c in b) { int k = hist[c]; r += Ch(c, 0) * k; g += Ch(c, 1) * k; bl += Ch(c, 2) * k; n += k; }
                pal.Add((int)((r + n / 2) / n) | (int)((g + n / 2) / n) << 5 | (int)((bl + n / 2) / n) << 10);
            }
            return pal;
        }

        static int Nearest(List<int> pal, int c)
        {
            int best = 0, bd = int.MaxValue;
            for (int i = 0; i < pal.Count; i++)
            {
                int d = 0;
                for (int k = 0; k < 3; k++) { int e = Ch(pal[i], k) - Ch(c, k); d += e * e; }
                if (d < bd) { bd = d; best = i; }
            }
            return best;
        }
    }

    // Packed GX command list (GBATEK "Geometry Commands"): one header word with up to four
    // command ids, followed by their parameters. Unused ids are NOP (0).
    public class GxList
    {
        public const byte MTX_RESTORE = 0x14, COLOR = 0x20, TEXCOORD = 0x22, VTX_16 = 0x23, BEGIN_VTXS = 0x40;
        readonly List<uint> words = new List<uint>();
        readonly List<byte> cmds = new List<byte>();
        readonly List<uint> args = new List<uint>();

        public void Cmd(byte id, params uint[] p)
        {
            cmds.Add(id); args.AddRange(p);
            if (cmds.Count == 4) Flush();
        }

        void Flush()
        {
            if (cmds.Count == 0) return;
            uint h = 0;
            for (int i = 0; i < cmds.Count; i++) h |= (uint)cmds[i] << (8 * i);
            words.Add(h); words.AddRange(args);
            cmds.Clear(); args.Clear();
        }

        // Every header holds a vertex command, so none is parameter-less (END_VTXS is a no-op and is omitted).
        public uint[] ToArray()
        {
            Flush();
            var r = new uint[words.Count + 1];
            r[0] = (uint)words.Count;
            words.CopyTo(r, 1);
            return r;
        }
    }

    public class DsModelReport
    {
        public string Name;
        public int Triangles, Draws, Textures, TextureBytes, ListBytes, UvClamped, Shift, Decals;
    }

    public static partial class Tool
    {
        // specs: "name=archive/file.mcms[#part]". Writes <outDir>/<name>.bin per spec.
        // Textures are halved (largest first) until all of them fit in vramBudget bytes.
        // groundSpecs: "name=model" writes <outDir>/<name>.bin with the floor triangles of an exported model.
        public static List<string> ExportDs(string input, string outDir, string[] specs, string[] animSpecs, string[] groundSpecs, int vramBudget, Action<string> log)
        {
            Directory.CreateDirectory(outDir);
            var archives = LoadArchives(input);
            var byId = new Dictionary<string, Dictionary<int, TexRef>>();
            var byFile = new Dictionary<string, List<TexRef>>();
            var files = new Dictionary<string, byte[]>(StringComparer.OrdinalIgnoreCase);
            foreach (var a in archives)
            {
                byId[a.Key] = new Dictionary<int, TexRef>();
                foreach (var e in Hiz.Read(a.Value))
                {
                    if (!Hiz.IsSafeName(e.Name)) continue;
                    var data = new byte[e.Size];
                    Buffer.BlockCopy(a.Value, e.Offset, data, 0, e.Size);
                    files[a.Key + "/" + e.Name] = data;
                    string ext = Path.GetExtension(e.Name).ToLowerInvariant();
                    if (ext != ".mct" && ext != ".mcts") continue;
                    var parts = Parts(ext, data, ".mct");
                    var list = byFile[a.Key + "/" + e.Name] = new List<TexRef>();
                    foreach (var p in parts)
                    {
                        if (!Mct.IsMct(data, p[0])) { list.Add(null); continue; }
                        var m = Mct.Parse(data, p[0]);
                        var r = new TexRef { Info = m, Src = data, Alpha = m.Format == 0x16, Ok = Mct.ExpectedSize(m) > 0 && m.DataSize >= Mct.ExpectedSize(m) };
                        list.Add(r);
                        if (m.Id != 0 && r.Ok) byId[a.Key][m.Id] = r;
                    }
                }
            }

            // Load the requested models (Item4/Item5: skeleton and skin, null when not skinned).
            var models = new List<Tuple<string, MeshStats, Func<int, TexRef>, Skeleton, SkinInfluence[]>>();
            var lods = new Dictionary<string, float>();
            foreach (var spec in specs)
            {
                int eq = spec.IndexOf('=');
                if (eq <= 0) throw new ArgumentException("model spec must be name=archive/file[#part]: " + spec);
                string name = spec.Substring(0, eq), path = spec.Substring(eq + 1);
                float lod = 1;
                int bang = path.IndexOf("!lod");
                if (bang > 0) { lod = int.Parse(path.Substring(bang + 4)) / 100f; path = path.Substring(0, bang); }
                lods[name] = lod;
                int part = 0, hash = path.IndexOf('#');
                if (hash >= 0) { part = int.Parse(path.Substring(hash + 1)); path = path.Substring(0, hash); }
                byte[] data;
                if (!files.TryGetValue(path, out data)) throw new FileNotFoundException("model not found: " + path);
                int slash = path.IndexOf('/');
                string arch = path.Substring(0, slash), file = path.Substring(slash + 1);
                var parts = Parts(Path.GetExtension(file).ToLowerInvariant(), data, ".mcm");
                if (part >= parts.Count) throw new ArgumentException("no part " + part + " in " + path);
                var s = Mcm.Parse(data, parts[part][0], parts[part][1], file);
                Func<int, TexRef> texOf = key =>
                {
                    var mat = s.Materials.Find(x => x.Key == key);
                    return mat == null ? null : ResolveTexture(byId, byFile, arch, file, mat.TextureId);
                };
                Skeleton skel = null;
                SkinInfluence[] skin = null;
                if (s.VertexBlocks.Count == 1 && s.VertexBlocks[0].Skinned)
                {
                    skel = Anim.ReadSkeleton(data, parts[part][0], parts[part][1]);
                    skin = Anim.ReadSkin(data, parts[part][0], parts[part][1], s.VertexBlocks[0].PositionCount);
                    if (skel == null || skin == null) { log("  " + name + ": skinned but skeleton/skin not found, exporting static"); skel = null; skin = null; }
                    else if (skel.Count > MaxDsNodes)
                    {
                        log(string.Format("  {0}: {1} nodes, the last {2} folded into their parents", name, skel.Count, skel.Count - MaxDsNodes));
                        TrimSkeleton(skel, skin);
                    }
                }
                models.Add(Tuple.Create(name, s, texOf, skel, skin));
            }

            // Animations: "name=archive/file.mcas#i,j,k" -> <outDir>/<name>.bin
            foreach (var spec in animSpecs ?? new string[0])
            {
                int eq = spec.IndexOf('='), hash = spec.IndexOf('#');
                if (eq <= 0 || hash < eq) throw new ArgumentException("animation spec must be name=archive/file.mcas#i,j: " + spec);
                string name = spec.Substring(0, eq), path = spec.Substring(eq + 1, hash - eq - 1);
                byte[] data;
                if (!files.TryGetValue(path, out data)) throw new FileNotFoundException("animation set not found: " + path);
                var parts = ChunkSet.Read(data);
                var list = new List<Animation>();
                Func<int, Animation> clip = i =>
                {
                    int consumed;
                    var a = i < parts.Count ? Anim.ReadAnimation(data, parts[i][0], parts[i][1], out consumed) : null;
                    if (a == null) throw new InvalidDataException("cannot read animation " + i + " of " + path + ": " + Anim.LastError);
                    return a;
                };
                foreach (var t in spec.Substring(hash + 1).Split(','))
                {
                    // "a~b@p": a still pose p percent of the way from clip a's first frame to clip b's.
                    int tilde = t.IndexOf('~'), at = t.IndexOf('@');
                    if (tilde > 0 && at > tilde)
                        list.Add(Anim.MixPoses(clip(int.Parse(t.Substring(0, tilde))), clip(int.Parse(t.Substring(tilde + 1, at - tilde - 1))), int.Parse(t.Substring(at + 1)) / 100f));
                    else
                        list.Add(clip(int.Parse(t)));
                }
                WriteAnimations(Path.Combine(outDir, name + ".bin"), list);
                log(string.Format("  {0}: {1} animations ({2} frames)", name, list.Count, string.Join(", ", list.ConvertAll(a => a.Frames.ToString()))));
            }

            // Texture budget across all exported models (8 bits per texel + palette).
            var used = new List<TexRef>();
            foreach (var m in models)
                foreach (var d in m.Item2.Draws)
                {
                    if (d.Vertices == null) continue;
                    var r = m.Item3(d.Material);
                    if (r != null && !used.Contains(r)) used.Add(r);
                }
            var shift = new Dictionary<TexRef, int>();
            foreach (var r in used) shift[r] = 0;
            Func<TexRef, int> size = r => Math.Max(8, r.Info.Width >> shift[r]) * Math.Max(8, r.Info.Height >> shift[r]) + 512;
            while (true)
            {
                int total = 0; TexRef big = null;
                foreach (var r in used) { total += size(r); if (big == null || size(r) > size(big)) big = r; }
                if (total <= vramBudget || big == null || size(big) <= 64 + 512) break;
                shift[big]++;
            }
            var converted = new Dictionary<TexRef, DsTexture>();
            foreach (var r in used)
            {
                int w, h;
                var rgba = DsTextures.Downscale(Mct.DecodeRgba(r.Src, r.Info), r.Info.Width, r.Info.Height, shift[r], out w, out h);
                converted[r] = DsTextures.ToRgb256(rgba, w, h);
                if (shift[r] > 0) log(string.Format("  texture id {0}: {1}x{2} -> {3}x{4}", r.Info.Id, r.Info.Width, r.Info.Height, w, h));
            }

            Directory.CreateDirectory(outDir);
            var report = new List<string>();
            report.Add(string.Format("{0,-10} {1,9} {2,6} {3,5} {4,8} {5,8} {6,6} {7,5} {8,6}", "model", "triangles", "draws", "tex", "texKB", "listKB", "shift", "uvClamp", "decals"));
            foreach (var m in models)
            {
                var rep = WriteModel(Path.Combine(outDir, m.Item1 + ".bin"), m.Item1, m.Item2, d => { var r = m.Item3(d); return r == null ? null : converted[r]; }, m.Item4, m.Item5, lods[m.Item1]);
                report.Add(string.Format("{0,-10} {1,9} {2,6} {3,5} {4,8:F1} {5,8:F1} {6,6} {7,5} {8,6}", rep.Name, rep.Triangles, rep.Draws, rep.Textures, rep.TextureBytes / 1024.0, rep.ListBytes / 1024.0, rep.Shift, rep.UvClamped, rep.Decals));
            }
            foreach (var spec in groundSpecs ?? new string[0])
            {
                int eq = spec.IndexOf('=');
                if (eq <= 0) throw new ArgumentException("ground spec must be name=model: " + spec);
                string name = spec.Substring(0, eq), model = spec.Substring(eq + 1);
                // "name=model+archive/stg_data_NN.bin" adds the game's own collision walls and floors.
                List<float[]> extra = null, extraFloors = null, rayFloors = null, rayWalls = null;
                int plus = model.IndexOf('+');
                if (plus > 0)
                {
                    byte[] sd;
                    if (!files.TryGetValue(model.Substring(plus + 1), out sd)) throw new FileNotFoundException("stage data not found: " + model.Substring(plus + 1));
                    // Section 1 (offset at +8): u32 groups, u32 triangles, u32, then 16 bytes per
                    // group (see StageDataWalls), then the triangles (x, y, z per corner).
                    extraFloors = new List<float[]>();
                    rayFloors = new List<float[]>();
                    rayWalls = new List<float[]>();
                    // Stage 12's data sits 468, 988 off its model (as its mission points do).
                    bool stage12 = model.Substring(plus + 1).EndsWith("stg_data_12.bin");
                    extra = StageDataWalls(sd, BitConverter.ToInt32(sd, 8), extraFloors, stage12 ? -468f : 0f, stage12 ? -988f : 0f, rayFloors, rayWalls, stage12);
                    model = model.Substring(0, plus);
                }
                var m = models.Find(x => x.Item1 == model);
                if (m == null) throw new ArgumentException("ground spec refers to a model not in -DsModel: " + model);
                // Draws with see-through texels (foliage cut-outs) don't make walls.
                Func<DrawRecord, bool> cutout = d => { var r = m.Item3(d.Material); return r != null && converted[r].Transparent; };
                report.Add(WriteGround(Path.Combine(outDir, name + ".bin"), name, m.Item2, 16f, cutout, extra, extraFloors, rayFloors, rayWalls));
            }
            return report;
        }

        static int F32(float v) { return (int)Math.Round(v * 4096); }

        public const int BoneSlotBase = 2;   // matrix stack slots 0/1 are used by model_draw()
        public const int MaxDsNodes = 28;    // MODEL_MAX_NODES (and slots BoneSlotBase..29 of 31)

        // Keeps the first MaxDsNodes nodes (Ashley's skirt bones, 20+, are leaves at the end):
        // vertices on a dropped node move with its nearest kept ancestor. Animations are cut
        // to the same nodes (WriteAnimations).
        static void TrimSkeleton(Skeleton s, SkinInfluence[] skin)
        {
            Func<int, int> kept = n => { while (n >= MaxDsNodes) n = s.Parent[n]; return n; };
            foreach (var inf in skin)
                if (inf != null)
                    for (int k = 0; k < inf.Nodes.Length; k++)
                        inf.Nodes[k] = kept(inf.Nodes[k]);
            Array.Resize(ref s.Parent, MaxDsNodes);
            Array.Resize(ref s.Local, MaxDsNodes);
        }

        // A lighter skinned mesh (-DsModel 'name=file!lodNN': NN percent of its triangles): the
        // shortest edges collapse, one end onto the other, while the triangles left keep their
        // facing. Only between vertices on the same bone; never a vertex on a texture seam (its
        // twin at the same place would stay behind: a crack), used by two draws, or on an open
        // border. The DS keeps 2048 polygons a frame: Leon (1157) and Ashley (944) took half.
        static List<DrawRecord> Decimate(List<DrawRecord> draws, VertexBlock sv, int[] bone, float keep)
        {
            var tris = new List<int[]>();   // draw, a, b, c
            for (int di = 0; di < draws.Count; di++)
                Mcm.ForEachTriangle(draws[di].PrimType, draws[di].Indices, (a, b, c) => { if (a != b && b != c && a != c) tris.Add(new[] { di, a, b, c }); });
            int target = (int)(tris.Count * keep);
            var locked = new HashSet<int>();
            var posUser = new Dictionary<int, HashSet<int>>();
            var drawOf = new Dictionary<int, int>();
            var edges = new Dictionary<long, int>();
            foreach (var t in tris)
                for (int k = 1; k <= 3; k++)
                {
                    int v = t[k], w = t[k % 3 + 1];
                    HashSet<int> set; int pi = sv.PosIndex[v];
                    if (!posUser.TryGetValue(pi, out set)) posUser[pi] = set = new HashSet<int>();
                    set.Add(v);
                    int dv; if (drawOf.TryGetValue(v, out dv) && dv != t[0]) locked.Add(v); drawOf[v] = t[0];
                    long key = (long)Math.Min(v, w) << 32 | (uint)Math.Max(v, w);
                    int n; edges.TryGetValue(key, out n); edges[key] = n + 1;
                }
            foreach (var set in posUser.Values) if (set.Count > 1) foreach (int v in set) locked.Add(v);
            foreach (var e in edges) if (e.Value == 1) { locked.Add((int)(e.Key >> 32)); locked.Add((int)(e.Key & 0xffffffff)); }
            Func<int, int, float> dist2 = (a, b) => { float x = sv.Pos[a * 3] - sv.Pos[b * 3], y = sv.Pos[a * 3 + 1] - sv.Pos[b * 3 + 1], z = sv.Pos[a * 3 + 2] - sv.Pos[b * 3 + 2]; return x * x + y * y + z * z; };
            Func<int, int, int, float[]> normal = (a, b, c) =>
            {
                float ux = sv.Pos[b * 3] - sv.Pos[a * 3], uy = sv.Pos[b * 3 + 1] - sv.Pos[a * 3 + 1], uz = sv.Pos[b * 3 + 2] - sv.Pos[a * 3 + 2];
                float vx = sv.Pos[c * 3] - sv.Pos[a * 3], vy = sv.Pos[c * 3 + 1] - sv.Pos[a * 3 + 1], vz = sv.Pos[c * 3 + 2] - sv.Pos[a * 3 + 2];
                float nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx, l = (float)Math.Sqrt(nx * nx + ny * ny + nz * nz);
                return l < 1e-9f ? null : new[] { nx / l, ny / l, nz / l };
            };
            var bad = new HashSet<long>();
            while (tris.Count > target)
            {
                int ba = -1, bb = -1; float best = float.MaxValue;
                foreach (var t in tris)
                    for (int k = 1; k <= 3; k++)
                        for (int dir = 0; dir < 2; dir++)
                        {
                            int a = dir == 0 ? t[k] : t[k % 3 + 1], b = dir == 0 ? t[k % 3 + 1] : t[k];
                            if (locked.Contains(a) || bone[a] != bone[b] || bad.Contains((long)a << 32 | (uint)b)) continue;
                            float c = dist2(a, b);
                            if (c < best) { best = c; ba = a; bb = b; }
                        }
                if (ba < 0) break;
                // The triangles around a, with a moved onto b: none may turn over or vanish.
                bool ok = true;
                foreach (var t in tris)
                {
                    int i = Array.IndexOf(t, ba, 1);
                    if (i < 0 || Array.IndexOf(t, bb, 1) > 0) continue;
                    var n0 = normal(t[1], t[2], t[3]);
                    var q = (int[])t.Clone(); q[i] = bb;
                    var n1 = normal(q[1], q[2], q[3]);
                    if (n0 == null || n1 == null || n0[0] * n1[0] + n0[1] * n1[1] + n0[2] * n1[2] < 0.5f) { ok = false; break; }
                }
                if (!ok) { bad.Add((long)ba << 32 | (uint)bb); continue; }
                tris.RemoveAll(t => Array.IndexOf(t, ba, 1) > 0 && Array.IndexOf(t, bb, 1) > 0);
                foreach (var t in tris) { int i = Array.IndexOf(t, ba, 1); if (i > 0) t[i] = bb; }
                locked.Add(ba);   // (gone)
                bad.Clear();
            }
            var outDraws = new List<DrawRecord>();
            for (int di = 0; di < draws.Count; di++)
            {
                var idx = new List<ushort>();
                foreach (var t in tris) if (t[0] == di) { idx.Add((ushort)t[1]); idx.Add((ushort)t[2]); idx.Add((ushort)t[3]); }
                var o = draws[di];
                outDraws.Add(new DrawRecord { Offset = o.Offset, PrimType = 4, IndexCount = idx.Count, TriCount = idx.Count / 3, MaxIndex = o.MaxIndex, Material = o.Material, Indices = idx.ToArray(), Vertices = o.Vertices, Lift = o.Lift });
            }
            return outDraws;
        }

        static DsModelReport WriteModel(string path, string name, MeshStats s, Func<int, DsTexture> texOf, Skeleton skel, SkinInfluence[] skin, float lod = 1)
        {
            var rep = new DsModelReport { Name = name };

            // Skinned: every vertex follows its heaviest bone (the DS can't blend matrices) and is
            // stored in that bone's bind space; the display list selects the bone's matrix with MTX_RESTORE.
            VertexBlock sv = skel != null ? s.VertexBlocks[0] : null;
            int[] bone = null;
            float[] local = null;
            if (sv != null)
            {
                var bindG = Anim.Globals(skel, i => skel.Local[i]);
                bone = new int[sv.VertexCount];
                local = new float[sv.VertexCount * 3];
                for (int i = 0; i < sv.VertexCount; i++)
                {
                    var inf = skin[sv.PosIndex[i]];
                    int best = 0;
                    for (int k = 1; k < inf.Weights.Length; k++) if (inf.Weights[k] > inf.Weights[best]) best = k;
                    bone[i] = inf.Nodes[best];
                    Anim.Apply(Anim.Inverse(bindG[bone[i]]), sv.Pos[i * 3], sv.Pos[i * 3 + 1], sv.Pos[i * 3 + 2], out local[i * 3], out local[i * 3 + 1], out local[i * 3 + 2]);
                }
            }
            var lo = new[] { float.MaxValue, float.MaxValue, float.MaxValue };
            var hi = new[] { float.MinValue, float.MinValue, float.MinValue };
            foreach (var d in s.Draws)
            {
                if (d.Vertices == null) continue;
                foreach (int i in d.Indices)
                    for (int k = 0; k < 3; k++) { float c = d.Vertices.Pos[i * 3 + k]; lo[k] = Math.Min(lo[k], c); hi[k] = Math.Max(hi[k], c); }
            }
            var center = new float[3];
            float ext = 0;
            if (local == null)
                for (int k = 0; k < 3; k++) { center[k] = (lo[k] + hi[k]) / 2; ext = Math.Max(ext, (hi[k] - lo[k]) / 2); }
            else
                foreach (var c in local) ext = Math.Max(ext, Math.Abs(c));
            // v16 is 4.12 fixed point: keep |coord| below 8.
            int shift = 0;
            while (ext / (1 << shift) >= 7.99f) shift++;
            rep.Shift = shift;
            float inv = 4096f / (1 << shift);

            var texList = new List<DsTexture>();
            var lists = new List<Tuple<int, bool, int, uint[]>>();
            var twoSided = new List<bool>();
            var chunkOf = new List<float[]>();
            // Opaque draws first, then draws with transparent texels.
            var order = new List<DrawRecord>(s.Draws.FindAll(d => d.Vertices != null));
            if (lod < 1 && sv != null && order.TrueForAll(d => d.Vertices == sv))
                order = Decimate(order, sv, bone, lod);
            bool chunked = skel == null && name.StartsWith("stage");
            if (skel == null) rep.Decals = MarkDecals(order);
            if (chunked) order = SplitChunks(order, ChunkSize);
            order.Sort((x, y) => { var a = texOf(x.Material); var b = texOf(y.Material); return (a != null && a.Transparent).CompareTo(b != null && b.Transparent); });
            foreach (var d in order)
            {
                var v = d.Vertices;
                var tex = texOf(d.Material);
                int ti = -1;
                if (tex != null) { ti = texList.IndexOf(tex); if (ti < 0) { ti = texList.Count; texList.Add(tex); } }
                var gx = new GxList();
                int lastColor = -1, lastBone = -1, tris = 0;
                if (v.Color == null) { gx.Cmd(GxList.COLOR, 0x7FFF); lastColor = 0x7FFF; }
                // Texcoords are 12.4 texels (+-2048): each strip / triangle is moved next to 0 by
                // whole repeats (the floor of its smallest s, t).
                Func<IList<int>, float[]> uvBase = ids =>
                {
                    float os = 0, ot = 0;
                    if (tex != null && v.Uv != null)
                    {
                        os = float.MaxValue; ot = float.MaxValue;
                        foreach (int i in ids) { os = Math.Min(os, v.Uv[i * 2]); ot = Math.Min(ot, v.Uv[i * 2 + 1]); }
                        os = (float)Math.Floor(os); ot = (float)Math.Floor(ot);
                    }
                    return new[] { os, ot };
                };
                Func<IList<int>, bool> uvFits = ids =>
                {
                    if (tex == null || v.Uv == null) return true;
                    var b0 = uvBase(ids);
                    foreach (int i in ids)
                        if ((v.Uv[i * 2] - b0[0]) * tex.W * 16 > 32767 || (v.Uv[i * 2 + 1] - b0[1]) * tex.H * 16 > 32767) return false;
                    return true;
                };
                Action<int, float, float> vertex = (i, os, ot) =>
                {
                    {
                        if (v.Color != null)
                        {
                            int col = DsTextures.Rgb15(v.Color[i * 4], v.Color[i * 4 + 1], v.Color[i * 4 + 2]);
                            if (col != lastColor) { gx.Cmd(GxList.COLOR, (uint)col); lastColor = col; }
                        }
                        if (tex != null && v.Uv != null)
                        {
                            int ss = (int)Math.Round((v.Uv[i * 2] - os) * tex.W * 16), tt = (int)Math.Round((v.Uv[i * 2 + 1] - ot) * tex.H * 16);
                            if (ss > 32767 || tt > 32767) { rep.UvClamped++; ss = Math.Min(ss, 32767); tt = Math.Min(tt, 32767); }
                            gx.Cmd(GxList.TEXCOORD, (uint)(ushort)ss | (uint)(ushort)tt << 16);
                        }
                        int x, y, z;
                        if (local != null && v == sv)
                        {
                            if (bone[i] != lastBone) { gx.Cmd(GxList.MTX_RESTORE, (uint)(BoneSlotBase + bone[i])); lastBone = bone[i]; }
                            x = Clamp16(local[i * 3] * inv); y = Clamp16(local[i * 3 + 1] * inv); z = Clamp16(local[i * 3 + 2] * inv);
                        }
                        else
                        {
                            x = Clamp16((v.Pos[i * 3] - center[0]) * inv); y = Clamp16((v.Pos[i * 3 + 1] + d.Lift - center[1]) * inv); z = Clamp16((v.Pos[i * 3 + 2] - center[2]) * inv);
                        }
                        gx.Cmd(GxList.VTX_16, (uint)(ushort)x | (uint)(ushort)y << 16, (uint)(ushort)z);
                    }
                };
                // Strips are kept as strips (BEGIN_VTXS 2): neighbouring triangles share their
                // vertices, which the DS's vertex RAM (6144 a frame) counts once. A source strip is
                // cut at its degenerate joins into runs; a run whose first triangle has the odd
                // winding starts with its first vertex twice (one empty triangle). Runs too wide in
                // texture space for 12.4 texcoords, and triangle lists, go out as triangles.
                var single = new List<int[]>();
                if (d.PrimType == 4)
                    Mcm.ForEachTriangle(d.PrimType, d.Indices, (a, b, c) => single.Add(new[] { a, b, c }));
                else
                {
                    var idx = d.Indices;
                    int rs = 0;
                    while (rs + 2 < idx.Length)
                    {
                        int e = rs + 2;
                        Func<int, bool> ok = i => idx[i - 2] != idx[i - 1] && idx[i - 1] != idx[i] && idx[i - 2] != idx[i];
                        if (!ok(e)) { rs++; continue; }
                        while (e + 1 < idx.Length && ok(e + 1)) e++;
                        var run = new List<int>();
                        for (int i = rs; i <= e; i++) run.Add(idx[i]);
                        if (uvFits(run))
                        {
                            var b0 = uvBase(run);
                            gx.Cmd(GxList.BEGIN_VTXS, 2);   // GL_TRIANGLE_STRIP
                            if (((rs + 2) & 1) != 0) vertex(run[0], b0[0], b0[1]);   // odd winding first
                            foreach (int i in run) vertex(i, b0[0], b0[1]);
                            tris += run.Count - 2;
                        }
                        else
                            for (int i = rs + 2; i <= e; i++)
                            {
                                int a = idx[i - 2], c = idx[i - 1], dd = idx[i];
                                single.Add((i & 1) == 0 ? new[] { a, c, dd } : new[] { c, a, dd });
                            }
                        rs = e - 1;   // the next run starts at this run's last edge
                    }
                }
                if (single.Count > 0)
                {
                    gx.Cmd(GxList.BEGIN_VTXS, 0);   // GL_TRIANGLES
                    foreach (var t in single)
                    {
                        var b0 = uvBase(t);
                        foreach (int i in t) vertex(i, b0[0], b0[1]);
                        tris++;
                    }
                }
                lists.Add(Tuple.Create(ti, tex != null && tex.Transparent, tris, gx.ToArray()));
                var mat = s.Materials.Find(x => x.Key == d.Material);
                twoSided.Add(mat != null && mat.Cull == 2);
                chunkOf.Add(chunked ? ChunkBounds(d) : null);
                rep.Triangles += tris;
            }
            rep.Draws = lists.Count;
            rep.Textures = texList.Count;

            var ground = Ground(s, (lo[0] + hi[0]) / 2, (lo[2] + hi[2]) / 2, lo[1]);
            using (var bw = new BinaryWriter(File.Create(path)))
            {
                bw.Write(Encoding.ASCII.GetBytes("RDS1"));
                bw.Write((ushort)texList.Count); bw.Write((ushort)lists.Count);
                foreach (var c in center) bw.Write(F32(c));
                bw.Write((ushort)shift); bw.Write((ushort)((skel != null ? 1 : 0) | (chunked ? 2 : 0)));   // flags: 1 = skinned, 2 = chunked
                foreach (var c in lo) bw.Write(F32(c));
                foreach (var c in hi) bw.Write(F32(c));
                bw.Write(F32((lo[0] + hi[0]) / 2)); bw.Write(F32(ground)); bw.Write(F32((lo[2] + hi[2]) / 2));
                bw.Write((uint)rep.Triangles);
                foreach (var t in texList)
                {
                    bw.Write((ushort)t.W); bw.Write((ushort)t.H);
                    bw.Write((byte)4);   // GL_RGB256
                    bw.Write((byte)(t.Transparent ? 1 : 0));
                    bw.Write((ushort)t.Palette.Length);
                    bw.Write((uint)t.Indices.Length);
                    bw.Write(t.Indices);
                    foreach (var c in t.Palette) bw.Write(c);
                    while (bw.BaseStream.Position % 4 != 0) bw.Write((byte)0);
                    rep.TextureBytes += t.Bytes;
                }
                for (int li = 0; li < lists.Count; li++)
                {
                    var l = lists[li];
                    bw.Write((ushort)(l.Item1 < 0 ? 0xFFFF : l.Item1));
                    bw.Write((byte)(l.Item2 ? 1 : 0)); bw.Write((byte)(twoSided[li] ? 1 : 0));   // transparent; two-sided
                    bw.Write((uint)l.Item3);
                    if (chunked) foreach (var c in chunkOf[li]) bw.Write(F32(c));
                    foreach (var w in l.Item4) bw.Write(w);
                    rep.ListBytes += l.Item4.Length * 4;
                }
                if (skel != null)
                {
                    // skeleton: u16 nodes, u16 slot base, per node: s16 parent, u16 0, s32 local[12] (f32, rotation rows + translation)
                    bw.Write((ushort)skel.Count); bw.Write((ushort)BoneSlotBase);
                    for (int i = 0; i < skel.Count; i++)
                    {
                        bw.Write((short)skel.Parent[i]); bw.Write((ushort)0);
                        foreach (var c in skel.Local[i]) bw.Write(F32(c));
                    }
                }
            }
            return rep;
        }

        // RDA1: char magic[4], u16 animations, u16 nodes, u32 offset[animations];
        // per animation: u16 frames, u16 0, then per node: u16 translationKeys, u16 rotationKeys,
        // keys of { u16 frame, u16 0, s32 x, y, z } (translation f32; rotation in libnds angle
        // units, 32768 per turn, unwrapped so linear interpolation takes the short way).
        // Carpets, rugs and stains lying on a floor (another draw's flat triangles just under them):
        // the DS's depth buffer can't tell two surfaces this close apart and shows them in
        // patches, so the smaller one is raised a little. Returns how many draws were.
        const float DecalLift = 0.15f, DecalGap = 0.3f;

        static int MarkDecals(List<DrawRecord> draws)
        {
            // Flat (upward) triangles of each draw: centroid, corners, height, area.
            var flat = new List<float[]>[draws.Count];
            var area = new float[draws.Count];
            var grid = new Dictionary<long, List<int[]>>();   // cell -> (draw, triangle)
            for (int di = 0; di < draws.Count; di++)
            {
                var d = draws[di];
                var v = d.Vertices;
                var list = new List<float[]>();
                flat[di] = list;
                Mcm.ForEachTriangle(d.PrimType, d.Indices, (a, b, c) =>
                {
                    float ax = v.Pos[a * 3], ay = v.Pos[a * 3 + 1], az = v.Pos[a * 3 + 2];
                    float bx = v.Pos[b * 3], by = v.Pos[b * 3 + 1], bz = v.Pos[b * 3 + 2];
                    float cx = v.Pos[c * 3], cy = v.Pos[c * 3 + 1], cz = v.Pos[c * 3 + 2];
                    float ux = bx - ax, uy = by - ay, uz = bz - az, wx = cx - ax, wy = cy - ay, wz = cz - az;
                    float nx = uy * wz - uz * wy, ny = uz * wx - ux * wz, nz = ux * wy - uy * wx;
                    float len = (float)Math.Sqrt(nx * nx + ny * ny + nz * nz);
                    if (len < 1e-4f || Math.Abs(ny) / len < 0.98f) return;
                    var t = new[] { (ax + bx + cx) / 3, (ay + by + cy) / 3, (az + bz + cz) / 3, ax, az, bx, bz, cx, cz, len / 2 };
                    area[di] += len / 2;
                    list.Add(t);
                    // (In every cell it covers: a big floor triangle reaches far from its centroid.)
                    int x0 = (int)Math.Floor(Math.Min(ax, Math.Min(bx, cx)) / 4), x1 = (int)Math.Floor(Math.Max(ax, Math.Max(bx, cx)) / 4);
                    int z0 = (int)Math.Floor(Math.Min(az, Math.Min(bz, cz)) / 4), z1 = (int)Math.Floor(Math.Max(az, Math.Max(bz, cz)) / 4);
                    for (int gx = x0; gx <= x1 && gx - x0 < 64; gx++)
                        for (int gz = z0; gz <= z1 && gz - z0 < 64; gz++)
                        {
                            long key = (long)gx * 100000 + gz;
                            List<int[]> cell;
                            if (!grid.TryGetValue(key, out cell)) grid[key] = cell = new List<int[]>();
                            cell.Add(new[] { di, list.Count - 1 });
                        }
                });
            }
            Func<float[], float, float, bool> inside = (t, x, z) =>
            {
                float d0 = (t[5] - t[3]) * (z - t[4]) - (t[6] - t[4]) * (x - t[3]);
                float d1 = (t[7] - t[5]) * (z - t[6]) - (t[8] - t[6]) * (x - t[5]);
                float d2 = (t[3] - t[7]) * (z - t[8]) - (t[4] - t[8]) * (x - t[7]);
                return (d0 >= 0 && d1 >= 0 && d2 >= 0) || (d0 <= 0 && d1 <= 0 && d2 <= 0);
            };
            int marked = 0;
            for (int di = 0; di < draws.Count; di++)
            {
                if (flat[di].Count == 0) continue;
                // Most of it flat, and most of that over another draw's (larger) floor.
                float over = 0;
                foreach (var t in flat[di])
                {
                    List<int[]> cell;
                    long key = (long)Math.Floor(t[0] / 4) * 100000 + (long)Math.Floor(t[2] / 4);
                    if (!grid.TryGetValue(key, out cell)) continue;
                    foreach (var r in cell)
                    {
                        if (r[0] == di || area[r[0]] <= area[di]) continue;
                        var u = flat[r[0]][r[1]];
                        if (Math.Abs(u[1] - t[1]) < DecalGap && inside(u, t[0], t[2])) { over += t[9]; break; }
                    }
                }
                if (over > area[di] * 0.5f)
                {
                    draws[di].Lift = DecalLift;
                    marked++;
                }
            }
            return marked;
        }

        // A stage's draws cut up by place, each texture's triangles into cells of this size, so that
        // only those near the camera need sending (the DS draws 2048 polygons a frame).
        const float ChunkSize = 72f;
        const int ChunkRun = 24;   // triangles to a strip at most (a long one would reach too far)

        static List<DrawRecord> SplitChunks(List<DrawRecord> draws, float size)
        {
            var o = new List<DrawRecord>();
            var grow = new Dictionary<VertexBlock, VertexGrowth>();
            foreach (var d in draws)
            {
                var v = d.Vertices;
                var cells = new Dictionary<long, List<int>>();
                var tris = new Dictionary<long, List<int>>();
                Func<IList<int>, long> cellOf = ids =>
                {
                    float x = 0, z = 0;
                    var pos = grow[v].Pos;   // (with the middles of cut edges)
                    foreach (int i in ids) { x += pos[i * 3]; z += pos[i * 3 + 2]; }
                    return (long)Math.Floor(x / ids.Count / size) * 100000 + (long)Math.Floor(z / ids.Count / size);
                };
                // Big triangles (an edge over MaxEdge) are cut into smaller ones, every long edge at
                // its middle: on a real DS a big triangle close to the camera can drop out whole (the
                // indoor stages' walls and floors). The middle of an edge is made once (by the edge's
                // two ends), so the triangles either side of it are cut alike: no cracks.
                VertexGrowth gv;
                if (!grow.TryGetValue(v, out gv)) grow[v] = gv = new VertexGrowth(v);
                Func<int, int, bool> tooLong = (a, b) =>
                {
                    float dx = gv.Pos[a * 3] - gv.Pos[b * 3], dy = gv.Pos[a * 3 + 1] - gv.Pos[b * 3 + 1], dz = gv.Pos[a * 3 + 2] - gv.Pos[b * 3 + 2];
                    return dx * dx + dy * dy + dz * dz > MaxEdge * MaxEdge;
                };
                Action<int, int, int, int> sub = null;
                sub = (a, b, c, depth) =>
                {
                    bool ab = depth < 6 && tooLong(a, b), bc = depth < 6 && tooLong(b, c), ca = depth < 6 && tooLong(c, a);
                    int n = (ab ? 1 : 0) + (bc ? 1 : 0) + (ca ? 1 : 0);
                    if (n == 0)
                    {
                        long k = cellOf(new[] { a, b, c });
                        if (!tris.ContainsKey(k)) tris[k] = new List<int>();
                        tris[k].Add(a); tris[k].Add(b); tris[k].Add(c);
                        return;
                    }
                    if (n == 3)
                    {
                        int m1 = gv.Mid(a, b), m2 = gv.Mid(b, c), m3 = gv.Mid(c, a);
                        sub(a, m1, m3, depth + 1); sub(m1, b, m2, depth + 1); sub(m3, m2, c, depth + 1); sub(m1, m2, m3, depth + 1);
                        return;
                    }
                    // One or two long edges: turned so that ab is long (and bc, if two).
                    if (n == 1 ? !ab : !(ab && bc))
                    {
                        if (n == 1 ? bc : (bc && ca)) { sub(b, c, a, depth); return; }
                        sub(c, a, b, depth); return;
                    }
                    if (n == 1)
                    {
                        int m = gv.Mid(a, b);
                        sub(a, m, c, depth + 1); sub(m, b, c, depth + 1);
                        return;
                    }
                    int p = gv.Mid(a, b), q = gv.Mid(b, c);
                    sub(p, b, q, depth + 1); sub(a, p, q, depth + 1); sub(a, q, c, depth + 1);
                };
                if (d.PrimType == 4)
                    Mcm.ForEachTriangle(d.PrimType, d.Indices, (a, b, c) => sub(a, b, c, 0));
                else
                {
                    var idx = d.Indices;
                    int rs = 0;
                    Func<int, bool> ok = i => idx[i - 2] != idx[i - 1] && idx[i - 1] != idx[i] && idx[i - 2] != idx[i];
                    while (rs + 2 < idx.Length)
                    {
                        if (!ok(rs + 2)) { rs++; continue; }
                        int e = rs + 2;
                        while (e + 1 < idx.Length && ok(e + 1) && e - rs < ChunkRun + 1) e++;
                        var run = new List<int>();
                        for (int i = rs; i <= e; i++) run.Add(idx[i]);
                        bool big = false;
                        for (int i = rs + 2; i <= e && !big; i++)
                            big = tooLong(idx[i - 2], idx[i - 1]) || tooLong(idx[i - 1], idx[i]) || tooLong(idx[i - 2], idx[i]);
                        if (big)
                        {
                            // As triangles (their windings as a strip has them), cut down to size.
                            for (int i = rs + 2; i <= e; i++)
                            {
                                int a = idx[i - 2], b = idx[i - 1], cc = idx[i];
                                if ((i & 1) == 0) sub(a, b, cc, 0); else sub(b, a, cc, 0);
                            }
                            rs = e - 1;
                            continue;
                        }
                        long k = cellOf(run);
                        if (!cells.ContainsKey(k)) cells[k] = new List<int>();
                        var c = cells[k];
                        // Runs joined by empty triangles, each starting on its old parity (its winding).
                        if (c.Count > 0) { c.Add(c[c.Count - 1]); c.Add(run[0]); c.Add(run[0]); }
                        if ((c.Count & 1) != (rs & 1)) c.Add(run[0]);
                        c.AddRange(run);
                        rs = e - 1;
                    }
                }
                foreach (var kv in cells)
                    o.Add(new DrawRecord { PrimType = d.PrimType, Material = d.Material, Vertices = v, Lift = d.Lift, Indices = kv.Value.ConvertAll(i => (ushort)i).ToArray() });
                foreach (var kv in tris)
                    o.Add(new DrawRecord { PrimType = 4, Material = d.Material, Vertices = v, Lift = d.Lift, Indices = kv.Value.ConvertAll(i => (ushort)i).ToArray() });
            }
            foreach (var kv in grow)
                kv.Value.Store();
            return o;
        }

        // (units; Leon is 18 tall. Environment RE4_MAXEDGE overrides it: a large value turns the
        // cutting off, as the stage files in ds/nitrofs were made.)
        static readonly float MaxEdge = float.Parse(Environment.GetEnvironmentVariable("RE4_MAXEDGE") ?? "24", System.Globalization.CultureInfo.InvariantCulture);

        // A vertex block that grows by edge middles (position, texcoord and colour halfway).
        class VertexGrowth
        {
            readonly VertexBlock v;
            public readonly List<float> Pos, Uv;
            readonly List<byte> Color;
            readonly Dictionary<long, int> mids = new Dictionary<long, int>();
            public VertexGrowth(VertexBlock b)
            {
                v = b;
                Pos = new List<float>(b.Pos);
                Uv = b.Uv != null ? new List<float>(b.Uv) : null;
                Color = b.Color != null ? new List<byte>(b.Color) : null;
            }
            public int Mid(int a, int b)
            {
                long key = a < b ? ((long)a << 32) | (uint)b : ((long)b << 32) | (uint)a;
                int m;
                if (mids.TryGetValue(key, out m)) return m;
                m = Pos.Count / 3;
                if (m > 65535) throw new InvalidDataException("too many vertices after cutting big triangles");
                for (int k = 0; k < 3; k++) Pos.Add((Pos[a * 3 + k] + Pos[b * 3 + k]) / 2);
                if (Uv != null) for (int k = 0; k < 2; k++) Uv.Add((Uv[a * 2 + k] + Uv[b * 2 + k]) / 2);
                if (Color != null) for (int k = 0; k < 4; k++) Color.Add((byte)((Color[a * 4 + k] + Color[b * 4 + k]) / 2));
                mids[key] = m;
                return m;
            }
            public void Store()
            {
                v.Pos = Pos.ToArray();
                if (Uv != null) v.Uv = Uv.ToArray();
                if (Color != null) v.Color = Color.ToArray();
                v.VertexCount = Pos.Count / 3;
            }
        }

        // Centre and radius of a chunk (world space).
        static float[] ChunkBounds(DrawRecord d)
        {
            var lo = new[] { float.MaxValue, float.MaxValue, float.MaxValue };
            var hi = new[] { float.MinValue, float.MinValue, float.MinValue };
            foreach (int i in d.Indices)
                for (int k = 0; k < 3; k++) { float c = d.Vertices.Pos[i * 3 + k]; lo[k] = Math.Min(lo[k], c); hi[k] = Math.Max(hi[k], c); }
            float[] m = { (lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, (lo[2] + hi[2]) / 2 };
            float r = 0;
            foreach (int i in d.Indices)
            {
                float x = d.Vertices.Pos[i * 3] - m[0], y = d.Vertices.Pos[i * 3 + 1] - m[1], z = d.Vertices.Pos[i * 3 + 2] - m[2];
                r = Math.Max(r, x * x + y * y + z * z);
            }
            return new[] { m[0], m[1], m[2], (float)Math.Sqrt(r) };
        }

        static void WriteAnimations(string path, List<Animation> anims)
        {
            int nodes = 0;
            foreach (var a in anims) nodes = Math.Max(nodes, a.Nodes.Length);
            nodes = Math.Min(nodes, MaxDsNodes);   // (as the models: see TrimSkeleton)
            using (var bw = new BinaryWriter(File.Create(path)))
            {
                bw.Write(Encoding.ASCII.GetBytes("RDA1"));
                bw.Write((ushort)anims.Count); bw.Write((ushort)nodes);
                long table = bw.BaseStream.Position;
                for (int i = 0; i < anims.Count; i++) bw.Write(0u);
                var offsets = new List<uint>();
                foreach (var a in anims)
                {
                    offsets.Add((uint)bw.BaseStream.Position);
                    bw.Write((ushort)a.Frames); bw.Write((ushort)0);
                    for (int n = 0; n < nodes; n++)
                    {
                        var chans = n < a.Nodes.Length ? a.Nodes[n] : new List<AnimChannel>();
                        var t = chans.Find(c => c.Kind == 1);
                        var r = chans.Find(c => c.Kind == 5);
                        bw.Write((ushort)(t == null ? 0 : t.Frames.Length));
                        bw.Write((ushort)(r == null ? 0 : r.Frames.Length));
                        if (t != null)
                            for (int k = 0; k < t.Frames.Length; k++)
                            {
                                bw.Write((ushort)t.Frames[k]); bw.Write((ushort)0);
                                foreach (var c in t.Values[k]) bw.Write(F32(c));
                            }
                        if (r != null)
                        {
                            var prev = new int[3];
                            for (int k = 0; k < r.Frames.Length; k++)
                            {
                                bw.Write((ushort)r.Frames[k]); bw.Write((ushort)0);
                                for (int j = 0; j < 3; j++)
                                {
                                    int ang = (int)Math.Round(r.Values[k][j] * 32768);
                                    if (k > 0) { while (ang - prev[j] > 16384) ang -= 32768; while (prev[j] - ang > 16384) ang += 32768; }
                                    prev[j] = ang;
                                    bw.Write(ang);
                                }
                            }
                        }
                    }
                }
                bw.BaseStream.Position = table;
                foreach (var o in offsets) bw.Write(o);
            }
        }

        static int Clamp16(float v) { return Math.Max(-32768, Math.Min(32767, (int)Math.Round(v))); }

        // RDG2 ground file, for height lookups and wall collision, on one uniform XZ grid.
        //   char magic[4] "RDG2"; u16 cellsX, cellsZ; s32 originX, originZ, cellSize (f32); u32 triangles;
        //   floors: triangles with |normal.y| >= 0.5 (at most 60 degrees steep)
        //     u32 cellStart[cellsX * cellsZ + 1]   (index into the triangle index list)
        //     u16 index[cellStart[last]], padded to 4 bytes
        //     triangles: s32 x0, z0, x1, z1, x2, z2 (f32, XZ footprint); s32 a, b, c (f32 plane
        //     y = a*x + b*z + c); 36 bytes each
        //   walls: steeper triangles (not from see-through draws) as their longest XZ edge
        //     u32 walls; u32 wallCellStart[cells + 1]; u16 wallIndex[...], padded to 4 bytes
        //     walls: s32 x0, z0, x1, z1, yMin, yMax (f32); 24 bytes each
        //     A wall is listed in every cell its segment comes within WallMargin of.
        const float WallMargin = 4f;

        // The game's own collision (stg_data_NN.bin section 1). Each group is { u8 id, u8 walk,
        // u8 camera, u8 shots, u8 kind, 3 bytes, u32 first, u32 count } (traced in bio4_brew.mod:
        // 0x64fc tests walkers against the groups with the walk byte set, 0x5728 and 0x71b8 the
        // camera byte, 0x5c80 the shots byte, each looping over all the groups). Only the walk
        // groups stop Leon: the others are house and roof detail for the camera and shots, and
        // groups with no byte set (kind 1 and 2: rails, door planes, trigger planes) stop nothing.
        // Stage 12's data against its model: moved by (-468, -988), then (Stage12Fix) a few units
        // more, a little less in x the further along z (read off its three doorways in the model:
        // the doors and the walls had stood 4-7 units off them).
        // The collision itself fits the model best with half of it (measured against the model's
        // walls near the three doorways: the game's collision outline is simpler than the model).
        public static void Stage12Fix(ref float x, ref float z, float part = 1) { x += (6.64f + 0.00374f * z) * part; z += 4.9f * part; }

        public static List<float[]> StageDataWalls(byte[] b, int sec, List<float[]> floors, float offX = 0, float offZ = 0, List<float[]> rayFloors = null, List<float[]> rayWalls = null, bool stage12 = false)
        {
            var o = new List<float[]>();
            int groups = BitConverter.ToInt32(b, sec), total = BitConverter.ToInt32(b, sec + 4);
            int first = sec + 12 + groups * 16;
            for (int g = 0; g < groups; g++)
            {
              int gp = sec + 12 + g * 16;
              // Rays: bit 0 the camera's groups, bit 1 the shots'; walkers: the walk groups.
              int mask = (b[gp + 2] != 0 ? 1 : 0) | (b[gp + 3] != 0 ? 2 : 0);
              bool walk = b[gp + 1] != 0;
              if (!walk && mask == 0) continue;
              int start = BitConverter.ToInt32(b, gp + 8), count = BitConverter.ToInt32(b, gp + 12);
              if (start < 0 || start + count > total) throw new InvalidDataException("collision group out of range");
              for (int t = start; t < start + count; t++)
              {
                int p = first + t * 36;
                var v = new float[9];
                for (int k = 0; k < 9; k++) v[k] = BitConverter.ToInt32(b, p + k * 4) / 256f;
                for (int k = 0; k < 3; k++) { v[k * 3] += offX; v[k * 3 + 2] += offZ; if (stage12) Stage12Fix(ref v[k * 3], ref v[k * 3 + 2], 0.5f); }
                float ux = v[3] - v[0], uy = v[4] - v[1], uz = v[5] - v[2], wx = v[6] - v[0], wy = v[7] - v[1], wz = v[8] - v[2];
                float nx = uy * wz - uz * wy, ny = uz * wx - ux * wz, nz = ux * wy - uy * wx;
                float len = (float)Math.Sqrt(nx * nx + ny * ny + nz * nz);
                if (len < 1e-4f) continue;
                if (Math.Abs(ny) / len >= 0.5f)
                {
                    if (walk && ny > 0) floors.Add(v);   // walkable: floors, ledges, stairs, the ground plane (not lintels above)
                    if (rayFloors != null && mask != 0) rayFloors.Add(new[] { v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8], mask });
                    continue;
                }
                int best = 0; float bl = -1;
                for (int k = 0; k < 3; k++)
                {
                    int n = (k + 1) % 3;
                    float dx = v[n * 3] - v[k * 3], dz = v[n * 3 + 2] - v[k * 3 + 2];
                    if (dx * dx + dz * dz > bl) { bl = dx * dx + dz * dz; best = k; }
                }
                if (bl < 0.01f) continue;
                int e = (best + 1) % 3;
                var seg = new[] { v[best * 3], v[best * 3 + 2], v[e * 3], v[e * 3 + 2], Math.Min(v[1], Math.Min(v[4], v[7])), Math.Max(v[1], Math.Max(v[4], v[7])) };
                if (walk) o.Add(seg);
                if (rayWalls != null && mask != 0) rayWalls.Add(new[] { seg[0], seg[1], seg[2], seg[3], seg[4], seg[5], mask });
              }
            }
            return o;
        }

        static string WriteGround(string path, string name, MeshStats s, float cell, Func<DrawRecord, bool> cutout, List<float[]> extraWalls = null, List<float[]> extraFloors = null, List<float[]> rayFloors = null, List<float[]> rayWalls = null)
        {
            var tris = new List<float[]>();    // x0 y0 z0 x1 y1 z1 x2 y2 z2
            var walls = new List<float[]>();   // x0 z0 x1 z1 yMin yMax
            if (extraWalls != null) walls.AddRange(extraWalls);
            float minX = float.MaxValue, minZ = float.MaxValue, maxX = float.MinValue, maxZ = float.MinValue;
            // With the game's collision the picture adds nothing (it isn't what the game walks on).
            foreach (var d in s.Draws)
            {
                if (extraWalls != null) break;
                var v = d.Vertices;
                if (v == null) continue;
                bool seeThrough = cutout(d);
                Mcm.ForEachTriangle(d.PrimType, d.Indices, (a, b, c) =>
                {
                    var t = new float[9];
                    int[] id = { a, b, c };
                    for (int k = 0; k < 3; k++) for (int j = 0; j < 3; j++) t[k * 3 + j] = v.Pos[id[k] * 3 + j];
                    float ux = t[3] - t[0], uy = t[4] - t[1], uz = t[5] - t[2], wx = t[6] - t[0], wy = t[7] - t[1], wz = t[8] - t[2];
                    float nx = uy * wz - uz * wy, ny = uz * wx - ux * wz, nz = ux * wy - uy * wx;
                    float len = (float)Math.Sqrt(nx * nx + ny * ny + nz * nz);
                    if (len < 1e-4f) return;
                    if (Math.Abs(ny) / len < 0.5f)
                    {
                        if (seeThrough) return;
                        // Longest edge seen from above; a vertical triangle projects onto it.
                        int best = 0; float bl = -1;
                        for (int k = 0; k < 3; k++)
                        {
                            int n = (k + 1) % 3;
                            float dx = t[n * 3] - t[k * 3], dz = t[n * 3 + 2] - t[k * 3 + 2];
                            if (dx * dx + dz * dz > bl) { bl = dx * dx + dz * dz; best = k; }
                        }
                        if (bl < 0.01f) return;
                        int e = (best + 1) % 3;
                        walls.Add(new[] { t[best * 3], t[best * 3 + 2], t[e * 3], t[e * 3 + 2],
                                          Math.Min(t[1], Math.Min(t[4], t[7])), Math.Max(t[1], Math.Max(t[4], t[7])) });
                        return;
                    }
                    tris.Add(t);
                    for (int k = 0; k < 3; k++) { minX = Math.Min(minX, t[k * 3]); maxX = Math.Max(maxX, t[k * 3]); minZ = Math.Min(minZ, t[k * 3 + 2]); maxZ = Math.Max(maxZ, t[k * 3 + 2]); }
                });
            }
            foreach (var t in extraFloors ?? new List<float[]>())
            {
                tris.Add(t);
                for (int k = 0; k < 3; k++) { minX = Math.Min(minX, t[k * 3]); maxX = Math.Max(maxX, t[k * 3]); minZ = Math.Min(minZ, t[k * 3 + 2]); maxZ = Math.Max(maxZ, t[k * 3 + 2]); }
            }
            if (tris.Count == 0) throw new InvalidDataException(name + ": no floor triangles");
            int cx = Math.Max(1, (int)Math.Ceiling((maxX - minX) / cell)), cz = Math.Max(1, (int)Math.Ceiling((maxZ - minZ) / cell));

            var cells = new List<int>[cx * cz];
            var wallCells = new List<int>[cx * cz];
            for (int i = 0; i < cells.Length; i++) { cells[i] = new List<int>(); wallCells[i] = new List<int>(); }
            Action<float, float, float, float, List<int>[], int> add = (lx, lz, hx, hz, grid, item) =>
            {
                int x0 = (int)Math.Floor((lx - minX) / cell), x1 = (int)Math.Floor((hx - minX) / cell);
                int z0 = (int)Math.Floor((lz - minZ) / cell), z1 = (int)Math.Floor((hz - minZ) / cell);
                for (int z = Math.Max(0, z0); z <= Math.Min(cz - 1, z1); z++)
                    for (int x = Math.Max(0, x0); x <= Math.Min(cx - 1, x1); x++) grid[z * cx + x].Add(item);
            };
            for (int i = 0; i < tris.Count; i++)
            {
                var t = tris[i];
                add(Math.Min(t[0], Math.Min(t[3], t[6])), Math.Min(t[2], Math.Min(t[5], t[8])),
                    Math.Max(t[0], Math.Max(t[3], t[6])), Math.Max(t[2], Math.Max(t[5], t[8])), cells, i);
            }
            for (int i = 0; i < walls.Count; i++)
            {
                var w = walls[i];
                add(Math.Min(w[0], w[2]) - WallMargin, Math.Min(w[1], w[3]) - WallMargin,
                    Math.Max(w[0], w[2]) + WallMargin, Math.Max(w[1], w[3]) + WallMargin, wallCells, i);
            }
            int maxPerCell = 0, maxWallsPerCell = 0;
            foreach (var c in cells) maxPerCell = Math.Max(maxPerCell, c.Count);
            foreach (var c in wallCells) maxWallsPerCell = Math.Max(maxWallsPerCell, c.Count);

            Action<BinaryWriter, List<int>[]> writeIndex = (bw, grid) =>
            {
                int start = 0;
                foreach (var c in grid) { bw.Write((uint)start); start += c.Count; }
                bw.Write((uint)start);
                foreach (var c in grid) foreach (var i in c) bw.Write((ushort)i);
                if (start % 2 != 0) bw.Write((ushort)0);
            };
            Action<BinaryWriter, List<float[]>, List<int>[], List<float[]>, List<int>[]> writeBody = (bw, ts, tc, ws, wc) =>
            {
                bw.Write((uint)ts.Count);
                writeIndex(bw, tc);
                foreach (var t in ts)
                {
                    for (int k = 0; k < 3; k++) { bw.Write(F32(t[k * 3])); bw.Write(F32(t[k * 3 + 2])); }
                    // Plane through the three points, solved for y.
                    float ux = t[3] - t[0], uy = t[4] - t[1], uz = t[5] - t[2], wx = t[6] - t[0], wy = t[7] - t[1], wz = t[8] - t[2];
                    float nx = uy * wz - uz * wy, ny = uz * wx - ux * wz, nz = ux * wy - uy * wx;
                    float a = -nx / ny, b = -nz / ny, c0 = t[1] - a * t[0] - b * t[2];
                    bw.Write(F32(a)); bw.Write(F32(b)); bw.Write(F32(c0));
                }
                bw.Write((uint)ws.Count);
                writeIndex(bw, wc);
                foreach (var w in ws) for (int k = 0; k < 6; k++) bw.Write(F32(w[k]));
            };
            using (var bw = new BinaryWriter(File.Create(path)))
            {
                bw.Write(Encoding.ASCII.GetBytes("RDG2"));
                bw.Write((ushort)cx); bw.Write((ushort)cz);
                bw.Write(F32(minX)); bw.Write(F32(minZ)); bw.Write(F32(cell));
                writeBody(bw, tris, cells, walls, wallCells);
                // Then what the rays hit (the same again on the same grid, then a byte per floor and
                // per wall: bit 0 the camera's, bit 1 the shots'), when the game's data gives it.
                if (rayFloors != null)
                {
                    var rc = new List<int>[cx * cz];
                    var rw = new List<int>[cx * cz];
                    for (int i = 0; i < rc.Length; i++) { rc[i] = new List<int>(); rw[i] = new List<int>(); }
                    for (int i = 0; i < rayFloors.Count; i++)
                    {
                        var t = rayFloors[i];
                        add(Math.Min(t[0], Math.Min(t[3], t[6])), Math.Min(t[2], Math.Min(t[5], t[8])),
                            Math.Max(t[0], Math.Max(t[3], t[6])), Math.Max(t[2], Math.Max(t[5], t[8])), rc, i);
                    }
                    for (int i = 0; i < rayWalls.Count; i++)
                    {
                        var w = rayWalls[i];
                        add(Math.Min(w[0], w[2]) - WallMargin, Math.Min(w[1], w[3]) - WallMargin,
                            Math.Max(w[0], w[2]) + WallMargin, Math.Max(w[1], w[3]) + WallMargin, rw, i);
                    }
                    writeBody(bw, rayFloors, rc, rayWalls, rw);
                    foreach (var t in rayFloors) bw.Write((byte)t[9]);
                    foreach (var w in rayWalls) bw.Write((byte)w[6]);
                }
            }
            return string.Format("{0,-10} floors {1} (max {2}/cell), walls {3} (max {4}/cell), grid {5}x{6} ({7} units)",
                                 name, tris.Count, maxPerCell, walls.Count, maxWallsPerCell, cx, cz, cell);
        }

        // Lowest triangle surface under (x, z), so roofs and treetops are skipped; falls back to the bbox bottom.
        static float Ground(MeshStats s, float x, float z, float fallback)
        {
            float best = float.MaxValue;
            foreach (var d in s.Draws)
            {
                var v = d.Vertices;
                if (v == null) continue;
                Mcm.ForEachTriangle(d.PrimType, d.Indices, (a, b, c) =>
                {
                    float ax = v.Pos[a * 3], az = v.Pos[a * 3 + 2], bx = v.Pos[b * 3], bz = v.Pos[b * 3 + 2], cx = v.Pos[c * 3], cz = v.Pos[c * 3 + 2];
                    float den = (bz - cz) * (ax - cx) + (cx - bx) * (az - cz);
                    if (Math.Abs(den) < 1e-6f) return;
                    float w0 = ((bz - cz) * (x - cx) + (cx - bx) * (z - cz)) / den;
                    float w1 = ((cz - az) * (x - cx) + (ax - cx) * (z - cz)) / den;
                    float w2 = 1 - w0 - w1;
                    if (w0 < 0 || w1 < 0 || w2 < 0) return;
                    best = Math.Min(best, w0 * v.Pos[a * 3 + 1] + w1 * v.Pos[b * 3 + 1] + w2 * v.Pos[c * 3 + 1]);
                });
            }
            return best == float.MaxValue ? fallback : best;
        }
    }
}
