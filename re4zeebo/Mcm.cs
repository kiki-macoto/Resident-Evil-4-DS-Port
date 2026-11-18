// RE4 Zeebo Edition の MCM のモデル：頂点 / 番号 / スキンのブロックの解析、OBJ の書き出し、プレビューの描画。
//
// MCM_ のチャンクはマテリアルの表で始まる：+0x0C に u16 の数、それから [u32 大きさ (0xCA)] の
// レコード、+8 に描画のキー（u32）、+0x34 にテクスチャの ID（u16）。
// その後にデータのブロックが続き、それぞれ「00000000 <u32 ノードの ID> FFFFFFFF 00000000 00」の
// タグが付き、中身で見分ける：
//   頂点のブロック：4バイトのスロット [u16 数][u8][u8] の60バイトの記述子
//             スロット0 位置（3 x 16.16）、スロット3 色（RGBA8）、スロット4 uv、スロット5 uv2
//             （2 x int16。値 = 生の値 * scale / 65536 + offset、scale は +28、uv の s/t の
//             offset は +32/+36）。データはスロットの順で並ぶ。
//   スキンのブロック：u16 位置の数、u16 ボーンの数 + u8 ボーンの ID、それから位置ごとに
//             [u8 影響数][u8 頂点数][u32 重み x n][u16 ボーン x n]。影響数0は「前の位置と同じ」
//             （2バイトのレコード）。そういうメッシュは各位置を1回だけ持ち、「頂点数」はそれを使う
//             連続した頂点（UV の継ぎ目で分かれる）の数を言う。剛体のメッシュ（武器）は同じブロックを
//             ボーン1つで使う。
// 描画のレコード（番号のブロックの中）：u32 prim（5 = ストリップ、4 = 一覧）、u16 1、u16 n、
// u16 マテリアルのキー、u16 0、u16 n、u16 indices[n]。
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text;

namespace Re4Zeebo
{
    public class DrawRecord
    {
        public int Offset, PrimType, IndexCount, TriCount, MaxIndex, Material;
        public ushort[] Indices;
        public VertexBlock Vertices;   // この描画が指す頂点のブロック（見つからなければ null）
        public float Lift;             // エクスポーター：これだけ持ち上げる（床の上の敷物かデカール）
    }

    public class VertexBlock
    {
        public int Offset, Node, PositionCount, VertexCount;
        public float[] Pos;            // 頂点ごと、xyz
        public float[] Uv;             // 頂点ごと、st（無ければ null）
        public byte[] Color;           // 頂点ごと、rgba（無ければ null）
        public bool Skinned;
        public float[] BindPos;        // スキン付きだけ：位置ごと、xyz（バインドポーズ）
        public int[] PosIndex;         // スキン付きだけ：頂点ごと、BindPos の番号
    }

    public class Material
    {
        public int Key, TextureId;
        public int Cull;   // バイト 0x24：2 = 両面から描く（中からも外からも見る壁）
    }

    public class MeshStats
    {
        public string Name;
        public int Triangles, Indices, DrawCalls, Vertices, UnmappedDraws;
        public List<DrawRecord> Draws = new List<DrawRecord>();
        public List<VertexBlock> VertexBlocks = new List<VertexBlock>();
        public List<Material> Materials = new List<Material>();
    }

    public static class Mcm
    {
        static readonly byte[] BlockTag = { 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0, 0, 0, 0, 0 };

        static bool TagAt(byte[] b, int o)
        {
            if (o < 6 || o + BlockTag.Length > b.Length) return false;
            for (int k = 0; k < BlockTag.Length; k++)
            {
                if (k >= 4 && k < 8) continue;   // ノードの ID
                if (b[o + k] != BlockTag[k]) return false;
            }
            return b[o + 6] == 0 && b[o + 7] == 0;
        }

        static float Fx(byte[] b, int o) { return BitConverter.ToInt32(b, o) / 65536f; }

        public static MeshStats Parse(byte[] b, int start, int len, string name)
        {
            var st = new MeshStats { Name = name };
            int end = start + len;
            ReadMaterials(b, start, end, st);

            var skins = new List<int>();
            var tags = new List<int[]>();   // 全ブロックの { オフセット, ノードの ID }
            for (int o = start + 6; o + 24 <= end; o++)
            {
                if (!TagAt(b, o)) continue;
                int node = BitConverter.ToInt32(b, o + 4);
                tags.Add(new[] { o, node });
                var v = ReadVertexBlock(b, o, end);
                if (v != null) { v.Offset = o - start; v.Node = node; st.VertexBlocks.Add(v); }
                else skins.Add(o);
            }
            // スキン付きのメッシュ：スキンのブロックの位置ごとの頂点数で、位置を頂点に広げる。
            foreach (var v in st.VertexBlocks)
            {
                if (v.PositionCount == v.VertexCount) continue;
                foreach (int s in skins)
                    if (ExpandSkinned(b, s + BlockTag.Length, end, v)) break;
            }

            ScanDraws(b, start, end, st);
            foreach (var d in st.Draws)
            {
                // 描画の番号のブロックは、自分より小さいノードの ID のうち一番大きい頂点のブロックを使う（頂点6 ->
                // 番号7と9。頂点11 -> 番号12）。代わり：その前の最後の頂点のブロック。
                int drawNode = -1;
                foreach (var t in tags) if (t[0] < start + d.Offset) drawNode = t[1];
                VertexBlock byNode = null, byOrder = null;
                foreach (var v in st.VertexBlocks)
                {
                    if (v.Pos == null || v.Pos.Length / 3 != v.VertexCount) continue;
                    if (v.Node < drawNode && (byNode == null || v.Node > byNode.Node)) byNode = v;
                    if (v.Offset < d.Offset) byOrder = v;
                }
                d.Vertices = byNode != null && d.MaxIndex < byNode.VertexCount ? byNode : byOrder;
                if (d.Vertices == null || d.MaxIndex >= d.Vertices.VertexCount) { d.Vertices = null; st.UnmappedDraws++; }
            }
            foreach (var v in st.VertexBlocks) st.Vertices += v.VertexCount;
            return st;
        }

        static void ReadMaterials(byte[] b, int start, int end, MeshStats st)
        {
            if (start + 0x0E > end || Encoding.ASCII.GetString(b, start, 4) != "MCM_") return;
            int count = BitConverter.ToUInt16(b, start + 0x0C);
            int p = start + 0x0E;
            for (int i = 0; i < count && p + 0x36 <= end; i++)
            {
                int size = BitConverter.ToInt32(b, p);
                if (size < 0x36 || p + size > end) break;
                st.Materials.Add(new Material { Key = BitConverter.ToInt32(b, p + 8), TextureId = BitConverter.ToUInt16(b, p + 0x34), Cull = b[p + 0x24] });
                p += size;
            }
        }

        static VertexBlock ReadVertexBlock(byte[] b, int tag, int end)
        {
            int size = BitConverter.ToInt32(b, tag - 4);
            int d = tag + BlockTag.Length;
            if (d + 60 > end || b[d + 2] != 1 || b[d + 3] != 3) return null;
            int cPos = BitConverter.ToUInt16(b, d), cCol = BitConverter.ToUInt16(b, d + 12);
            int cUv = BitConverter.ToUInt16(b, d + 16), cUv2 = BitConverter.ToUInt16(b, d + 20);
            if (BitConverter.ToUInt16(b, d + 4) != 0 || BitConverter.ToUInt16(b, d + 8) != 0 || BitConverter.ToUInt16(b, d + 24) != 0) return null;
            int payload = tag - 4 + size - (d + 60);
            if (cPos == 0 || payload != cPos * 12 + (cCol + cUv + cUv2) * 4) return null;

            var v = new VertexBlock { PositionCount = cPos, VertexCount = cUv > 0 ? cUv : cPos };
            int p = d + 60;
            v.Pos = new float[cPos * 3];
            for (int i = 0; i < cPos * 3; i++) v.Pos[i] = Fx(b, p + i * 4);
            p += cPos * 12;
            if (cCol > 0)
            {
                v.Color = new byte[cCol * 4];
                Buffer.BlockCopy(b, p, v.Color, 0, cCol * 4);
                p += cCol * 4;
            }
            if (cUv > 0)
            {
                float scale = b[d + 28] / 65536f, so = Fx(b, d + 32), to = Fx(b, d + 36);
                v.Uv = new float[cUv * 2];
                for (int i = 0; i < cUv; i++)
                {
                    v.Uv[i * 2] = BitConverter.ToInt16(b, p + i * 4) * scale + so;
                    v.Uv[i * 2 + 1] = BitConverter.ToInt16(b, p + i * 4 + 2) * scale + to;
                }
            }
            if (v.Color != null && cCol != v.VertexCount) v.Color = null;
            return v;
        }

        static bool ExpandSkinned(byte[] b, int d, int end, VertexBlock v)
        {
            if (d + 4 > end || BitConverter.ToUInt16(b, d) != v.PositionCount) return false;
            int p = d + 4 + BitConverter.ToUInt16(b, d + 2);
            var map = new int[v.VertexCount];
            int vi = 0;
            for (int i = 0; i < v.PositionCount; i++)
            {
                if (p + 2 > end) return false;
                int inf = b[p], dup = b[p + 1];   // inf 0：前の位置と同じ影響
                if ((inf == 0 && i == 0) || vi + dup > v.VertexCount) return false;
                for (int k = 0; k < dup; k++) map[vi++] = i;
                p += 2 + 6 * inf;
            }
            if (vi != v.VertexCount) return false;
            var pos = new float[v.VertexCount * 3];
            for (int i = 0; i < v.VertexCount; i++)
                for (int k = 0; k < 3; k++) pos[i * 3 + k] = v.Pos[map[i] * 3 + k];
            v.BindPos = v.Pos;
            v.PosIndex = map;
            v.Pos = pos;
            v.Skinned = true;
            return true;
        }

        // レコードは揃っていないので、全部のオフセットを試す：当たりには n が2回、その間に0、まともな番号が
        // 要る。三角形は番号から数える（ストリップの縮退したつなぎ目は除く）。
        static void ScanDraws(byte[] b, int start, int end, MeshStats st)
        {
            for (int o = start; o + 14 <= end; o++)
            {
                int prim = BitConverter.ToInt32(b, o);
                if (prim != 4 && prim != 5) continue;
                int f4 = BitConverter.ToUInt16(b, o + 4);
                int n = BitConverter.ToUInt16(b, o + 6);
                if (f4 == 0 || f4 > 64 || n < 3 || BitConverter.ToUInt16(b, o + 10) != 0 || BitConverter.ToUInt16(b, o + 12) != n) continue;
                if (o + 14 + n * 2 > end || (prim == 4 && n % 3 != 0)) continue;
                var idx = new ushort[n];
                int maxIdx = 0;
                for (int i = 0; i < n; i++) { idx[i] = BitConverter.ToUInt16(b, o + 14 + i * 2); maxIdx = Math.Max(maxIdx, idx[i]); }
                if (maxIdx >= 8192) continue;
                int tris = 0;
                ForEachTriangle(prim, idx, (x, y, z) => tris++);
                if (tris == 0) continue;
                st.Draws.Add(new DrawRecord { Offset = o - start, PrimType = prim, IndexCount = n, TriCount = tris, MaxIndex = maxIdx, Material = BitConverter.ToUInt16(b, o + 8), Indices = idx });
                st.Triangles += tris;
                st.Indices += n;
                st.DrawCalls++;
                o += 13 + n * 2;
            }
        }

        // ストリップは巻き方が交互。縮退したつなぎ目は飛ばす。出力は GL の反時計回り。
        public static void ForEachTriangle(int prim, ushort[] idx, Action<int, int, int> f)
        {
            if (prim == 4)
            {
                for (int i = 0; i + 2 < idx.Length; i += 3) f(idx[i], idx[i + 1], idx[i + 2]);
                return;
            }
            for (int i = 2; i < idx.Length; i++)
            {
                int a = idx[i - 2], c = idx[i - 1], d = idx[i];
                if (a == c || c == d || a == d) continue;
                if ((i & 1) == 0) f(a, c, d); else f(c, a, d);
            }
        }
    }

    public static class ObjExport
    {
        static string F(float v) { return v.ToString("0.######", CultureInfo.InvariantCulture); }

        // texturePath(materialKey) は OBJ のフォルダーからの相対の PNG のパス（または null）と、テクスチャに
        // アルファがあるかを返す。
        public static void Write(string objPath, MeshStats m, Func<int, Tuple<string, bool>> texturePath)
        {
            string mtlName = Path.GetFileNameWithoutExtension(objPath) + ".mtl";
            var obj = new StringBuilder();
            var mtl = new StringBuilder();
            obj.AppendLine("# " + m.Name + " (RE4 Zeebo Edition MCM)");
            obj.AppendLine("mtllib " + mtlName);
            var done = new HashSet<int>();
            var baseOf = new Dictionary<VertexBlock, int>();
            int next = 1;
            foreach (var v in m.VertexBlocks)
            {
                if (v.Pos.Length / 3 != v.VertexCount) continue;
                baseOf[v] = next;
                for (int i = 0; i < v.VertexCount; i++)
                {
                    obj.Append("v ").Append(F(v.Pos[i * 3])).Append(' ').Append(F(v.Pos[i * 3 + 1])).Append(' ').Append(F(v.Pos[i * 3 + 2]));
                    if (v.Color != null)
                        obj.Append(' ').Append(F(v.Color[i * 4] / 255f)).Append(' ').Append(F(v.Color[i * 4 + 1] / 255f)).Append(' ').Append(F(v.Color[i * 4 + 2] / 255f));
                    obj.AppendLine();
                }
                for (int i = 0; i < v.VertexCount; i++)
                {
                    // GL の t は画像の上から下へ、OBJ の v は下から上へ。
                    float s = v.Uv != null ? v.Uv[i * 2] : 0, t = v.Uv != null ? v.Uv[i * 2 + 1] : 0;
                    obj.Append("vt ").Append(F(s)).Append(' ').Append(F(-t)).AppendLine();
                }
                next += v.VertexCount;
            }
            int n = 0;
            foreach (var d in m.Draws)
            {
                n++;
                if (d.Vertices == null || !baseOf.ContainsKey(d.Vertices)) continue;
                int b0 = baseOf[d.Vertices];
                string mat = "mat_" + d.Material;
                obj.AppendLine("g draw_" + n.ToString("D2"));
                obj.AppendLine("usemtl " + mat);
                ForEachFace(d, (x, y, z) => obj.AppendFormat("f {0}/{0} {1}/{1} {2}/{2}\n", b0 + x, b0 + y, b0 + z));
                if (done.Add(d.Material))
                {
                    mtl.AppendLine("newmtl " + mat);
                    mtl.AppendLine("Kd 1 1 1");
                    var tex = texturePath(d.Material);
                    if (tex != null)
                    {
                        mtl.AppendLine("map_Kd " + tex.Item1);
                        if (tex.Item2) mtl.AppendLine("map_d " + tex.Item1);
                    }
                    mtl.AppendLine();
                }
            }
            File.WriteAllText(objPath, obj.ToString());
            File.WriteAllText(Path.Combine(Path.GetDirectoryName(objPath), mtlName), mtl.ToString());
        }

        static void ForEachFace(DrawRecord d, Action<int, int, int> f) { Mcm.ForEachTriangle(d.PrimType, d.Indices, f); }
    }

    // ソフトウェアのプレビュー：正射影、テクスチャ付き、Z バッファ。正面から（+Z がカメラの方）、または
    // ステージのマップのような平らなモデルなら上から（+Y がカメラの方）。
    public static class Preview
    {
        public class Tex { public int W, H; public byte[] Rgba; }

        public static void Render(string pngPath, MeshStats m, Func<int, Tex> textureFor, int size)
        {
            var lo = new[] { float.MaxValue, float.MaxValue, float.MaxValue };
            var hi = new[] { float.MinValue, float.MinValue, float.MinValue };
            foreach (var d in m.Draws)
            {
                if (d.Vertices == null) continue;
                foreach (int i in d.Indices)
                    for (int k = 0; k < 3; k++) { float c = d.Vertices.Pos[i * 3 + k]; lo[k] = Math.Min(lo[k], c); hi[k] = Math.Max(hi[k], c); }
            }
            if (lo[0] > hi[0]) return;
            bool top = hi[1] - lo[1] < 0.25f * Math.Max(hi[0] - lo[0], hi[2] - lo[2]);
            // 画面の軸：x -> 右。上 = y（正面）か -z（上）。深度 = z（正面）か y（上）。
            int ay = top ? 2 : 1, az = top ? 1 : 2;
            float sgn = top ? -1 : 1;
            float minX = lo[0], maxX = hi[0];
            float minY = top ? -hi[2] : lo[1], maxY = top ? -lo[2] : hi[1];
            float span = Math.Max(maxX - minX, maxY - minY) * 1.05f + 1e-6f;
            float cx = (minX + maxX) / 2, cy = (minY + maxY) / 2;
            int W = size, H = size;
            var img = new byte[W * H * 4];
            for (int i = 0; i < W * H; i++) { img[i * 4] = 48; img[i * 4 + 1] = 48; img[i * 4 + 2] = 56; img[i * 4 + 3] = 255; }
            var zb = new float[W * H];
            for (int i = 0; i < zb.Length; i++) zb[i] = float.MinValue;

            foreach (var d in m.Draws)
            {
                var v = d.Vertices;
                if (v == null) continue;
                var tex = textureFor(d.Material);
                Mcm.ForEachTriangle(d.PrimType, d.Indices, (a, b2, c) =>
                {
                    int[] id = { a, b2, c };
                    var sx = new float[3]; var sy = new float[3]; var sz = new float[3];
                    for (int k = 0; k < 3; k++)
                    {
                        sx[k] = (v.Pos[id[k] * 3] - cx) / span * W + W / 2f;
                        sy[k] = H / 2f - (sgn * v.Pos[id[k] * 3 + ay] - cy) / span * H;
                        sz[k] = v.Pos[id[k] * 3 + az];
                    }
                    float area = (sx[1] - sx[0]) * (sy[2] - sy[0]) - (sx[2] - sx[0]) * (sy[1] - sy[0]);
                    if (Math.Abs(area) < 1e-6f) return;
                    int x0 = Math.Max(0, (int)Math.Floor(Math.Min(sx[0], Math.Min(sx[1], sx[2])))), x1 = Math.Min(W - 1, (int)Math.Ceiling(Math.Max(sx[0], Math.Max(sx[1], sx[2]))));
                    int y0 = Math.Max(0, (int)Math.Floor(Math.Min(sy[0], Math.Min(sy[1], sy[2])))), y1 = Math.Min(H - 1, (int)Math.Ceiling(Math.Max(sy[0], Math.Max(sy[1], sy[2]))));
                    for (int y = y0; y <= y1; y++)
                        for (int x = x0; x <= x1; x++)
                        {
                            float px = x + 0.5f, py = y + 0.5f;
                            float w0 = ((sx[1] - px) * (sy[2] - py) - (sx[2] - px) * (sy[1] - py)) / area;
                            float w1 = ((sx[2] - px) * (sy[0] - py) - (sx[0] - px) * (sy[2] - py)) / area;
                            float w2 = 1 - w0 - w1;
                            if (w0 < 0 || w1 < 0 || w2 < 0) continue;
                            float z = w0 * sz[0] + w1 * sz[1] + w2 * sz[2];
                            int pi = y * W + x;
                            if (z <= zb[pi]) continue;
                            float r = 200, g = 200, bl = 200, al = 255;
                            if (tex != null && v.Uv != null)
                            {
                                float s = w0 * v.Uv[a * 2] + w1 * v.Uv[b2 * 2] + w2 * v.Uv[c * 2];
                                float t = w0 * v.Uv[a * 2 + 1] + w1 * v.Uv[b2 * 2 + 1] + w2 * v.Uv[c * 2 + 1];
                                int tx = (int)((s - Math.Floor(s)) * tex.W) % tex.W, ty = (int)((t - Math.Floor(t)) * tex.H) % tex.H;
                                int ti = (ty * tex.W + tx) * 4;
                                r = tex.Rgba[ti]; g = tex.Rgba[ti + 1]; bl = tex.Rgba[ti + 2]; al = tex.Rgba[ti + 3];
                            }
                            if (al < 128) continue;
                            if (v.Color != null)
                            {
                                // GL_MODULATE：テクスチャ * 頂点の色。
                                r *= (w0 * v.Color[a * 4] + w1 * v.Color[b2 * 4] + w2 * v.Color[c * 4]) / 255f;
                                g *= (w0 * v.Color[a * 4 + 1] + w1 * v.Color[b2 * 4 + 1] + w2 * v.Color[c * 4 + 1]) / 255f;
                                bl *= (w0 * v.Color[a * 4 + 2] + w1 * v.Color[b2 * 4 + 2] + w2 * v.Color[c * 4 + 2]) / 255f;
                            }
                            zb[pi] = z;
                            img[pi * 4] = (byte)Math.Min(255, r); img[pi * 4 + 1] = (byte)Math.Min(255, g); img[pi * 4 + 2] = (byte)Math.Min(255, bl);
                        }
                });
            }
            Png.Write(pngPath, W, H, img);
        }
    }
}
