// RE4 Zeebo Edition skeletons (in MCM), skin weights and MCA animations, plus CPU skinning.
//
// Skeleton block (inside an MCM_ chunk, same "00000000 <node> FFFFFFFF 00000000" tag as the
// other blocks): u16 node count at tag+16, then per node 91 bytes:
//   s16 parent, s32 m[12] (16.16: 3x3 rotation row-major, then translation), s32 scale[3],
//   u8 flag, s32 bboxMin[3], s32 bboxMax[3], s32 unknown.
// Skin block: see Mcm.cs. Bone numbers in its records index its bone list, which holds node ids.
// MCA_ chunk: u16 frame count at +0x29, u16 node count at +0x2D, node records from +0x2F:
//   u16 channel count, then per channel: u8 kind (1 = translation, 5 = rotation), u8 3,
//   u8 key count, 5 bytes, keys of [u8 interpolation][u8 vectors][u16 0][u16 frame] and
//   `vectors` x (s32 x, y, z): 1 = value (linear keys), 3 = value + in/out tangents
//   (hermite keys, interpolated here as linear).
using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace Re4Zeebo
{
    public class Skeleton
    {
        public int[] Parent;
        public float[][] Local;   // 4x3 per node: r00 r01 r02 r10 r11 r12 r20 r21 r22 tx ty tz
        public int Count { get { return Parent.Length; } }
    }

    public class SkinInfluence { public int[] Nodes; public float[] Weights; }

    public class AnimChannel
    {
        public int Kind;
        public int[] Frames;
        public float[][] Values;   // raw 16.16 values as floats
    }

    public class Animation
    {
        public int Frames;
        public List<AnimChannel>[] Nodes;
    }

    public static class Anim
    {
        static float Fx(byte[] b, int o) { return BitConverter.ToInt32(b, o) / 65536f; }

        static bool Tag(byte[] b, int o)
        {
            return o >= 6 && o + 16 <= b.Length && BitConverter.ToInt32(b, o) == 0 && b[o + 6] == 0 && b[o + 7] == 0
                && BitConverter.ToInt32(b, o + 8) == -1 && BitConverter.ToInt32(b, o + 12) == 0;
        }

        public static Skeleton ReadSkeleton(byte[] b, int start, int len)
        {
            int end = start + len;
            for (int o = start + 6; o + 18 <= end; o++)
            {
                if (!Tag(b, o)) continue;
                int size = BitConverter.ToInt32(b, o - 4), n = BitConverter.ToUInt16(b, o + 16);
                if (n == 0 || n > 128 || o + 18 + n * 91 != o - 4 + size) continue;
                var s = new Skeleton { Parent = new int[n], Local = new float[n][] };
                int p = o + 18;
                for (int i = 0; i < n; i++, p += 91)
                {
                    s.Parent[i] = BitConverter.ToInt16(b, p);
                    s.Local[i] = new float[12];
                    for (int k = 0; k < 12; k++) s.Local[i][k] = Fx(b, p + 2 + k * 4);
                }
                return s;
            }
            return null;
        }

        // Per position: node ids and weights (same block Mcm.ExpandSkinned uses).
        public static SkinInfluence[] ReadSkin(byte[] b, int start, int len, int positions)
        {
            int end = start + len;
            for (int o = start + 6; o + 20 <= end; o++)
            {
                if (!Tag(b, o)) continue;
                int d = o + 17;
                if (BitConverter.ToUInt16(b, d) != positions) continue;
                int nb = BitConverter.ToUInt16(b, d + 2);
                var bones = new int[nb];
                for (int i = 0; i < nb; i++) bones[i] = b[d + 4 + i];
                int p = d + 4 + nb;
                var r = new SkinInfluence[positions];
                bool ok = true;
                for (int i = 0; i < positions && ok; i++)
                {
                    int inf = b[p];
                    if (inf == 0 && i > 0) { r[i] = r[i - 1]; p += 2; continue; }   // same as previous
                    if (inf == 0 || inf > 4 || p + 2 + 6 * inf > end) { ok = false; break; }
                    var s = new SkinInfluence { Nodes = new int[inf], Weights = new float[inf] };
                    for (int k = 0; k < inf; k++)
                    {
                        s.Weights[k] = Fx(b, p + 2 + k * 4);
                        int bi = BitConverter.ToUInt16(b, p + 2 + inf * 4 + k * 2);
                        if (bi >= nb) { ok = false; break; }
                        s.Nodes[k] = bones[bi];
                    }
                    r[i] = s;
                    p += 2 + 6 * inf;
                }
                if (ok) return r;
            }
            return null;
        }

        public static string LastError;

        public static Animation ReadAnimation(byte[] b, int start, int len, out int consumed)
        {
            consumed = 0;
            if (Encoding.ASCII.GetString(b, start, 4) != "MCA_") return null;
            var a = new Animation { Frames = BitConverter.ToUInt16(b, start + 0x29) };
            int n = BitConverter.ToUInt16(b, start + 0x2D);
            a.Nodes = new List<AnimChannel>[n];
            int p = start + 0x2F, end = start + len;
            for (int i = 0; i < n; i++)
            {
                if (p + 2 > end) { LastError = "node " + i + " at +0x" + (p - start).ToString("x"); return null; }
                int nc = BitConverter.ToUInt16(b, p); p += 2;
                a.Nodes[i] = new List<AnimChannel>();
                for (int c = 0; c < nc; c++)
                {
                    if (p + 8 > end) { LastError = "node " + i + " chan " + c + " at +0x" + (p - start).ToString("x"); return null; }
                    var ch = new AnimChannel { Kind = b[p] };
                    int comps = b[p + 1], keys = b[p + 2];
                    p += 8;
                    ch.Frames = new int[keys];
                    ch.Values = new float[keys][];
                    for (int k = 0; k < keys; k++)
                    {
                        // Key: [u8 interpolation][u8 vectors][u16 0][u16 frame] then `vectors`
                        // vectors of `comps` values: the value, plus in/out tangents when 3.
                        int vectors = p + 2 <= end ? b[p + 1] : 0;
                        if (vectors == 0 || p + 6 + vectors * comps * 4 > end) { LastError = "node " + i + " chan " + c + " key " + k + " at +0x" + (p - start).ToString("x"); return null; }
                        ch.Frames[k] = BitConverter.ToUInt16(b, p + 4);
                        ch.Values[k] = new float[comps];
                        for (int j = 0; j < comps; j++) ch.Values[k][j] = Fx(b, p + 6 + j * 4);
                        p += 6 + vectors * comps * 4;
                    }
                    a.Nodes[i].Add(ch);
                }
            }
            consumed = p - start;
            return a;
        }

        public static float[] Sample(AnimChannel ch, float frame)
        {
            int k = 0;
            while (k + 1 < ch.Frames.Length && ch.Frames[k + 1] <= frame) k++;
            if (k + 1 >= ch.Frames.Length || ch.Frames[k + 1] == ch.Frames[k]) return (float[])ch.Values[k].Clone();
            float t = (frame - ch.Frames[k]) / (ch.Frames[k + 1] - ch.Frames[k]);
            var r = new float[ch.Values[k].Length];
            for (int j = 0; j < r.Length; j++) r[j] = ch.Values[k][j] + (ch.Values[k + 1][j] - ch.Values[k][j]) * t;
            return r;
        }

        // 4x3 helpers (rotation rows + translation).
        public static float[] Mul(float[] a, float[] b)   // a * b (apply b first)
        {
            var r = new float[12];
            for (int i = 0; i < 3; i++)
                for (int j = 0; j < 3; j++)
                    r[i * 3 + j] = a[i * 3] * b[j] + a[i * 3 + 1] * b[3 + j] + a[i * 3 + 2] * b[6 + j];
            for (int i = 0; i < 3; i++)
                r[9 + i] = a[i * 3] * b[9] + a[i * 3 + 1] * b[10] + a[i * 3 + 2] * b[11] + a[9 + i];
            return r;
        }

        public static float[] Inverse(float[] m)   // rigid transforms only
        {
            var r = new float[12];
            for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) r[i * 3 + j] = m[j * 3 + i];
            for (int i = 0; i < 3; i++) r[9 + i] = -(r[i * 3] * m[9] + r[i * 3 + 1] * m[10] + r[i * 3 + 2] * m[11]);
            return r;
        }

        public static void Apply(float[] m, float x, float y, float z, out float ox, out float oy, out float oz)
        {
            ox = m[0] * x + m[1] * y + m[2] * z + m[9];
            oy = m[3] * x + m[4] * y + m[5] * z + m[10];
            oz = m[6] * x + m[7] * y + m[8] * z + m[11];
        }

        // Rotation units/order are configurable while they're being worked out.
        public static float AngleScale = (float)(Math.PI * 2);   // raw (16.16) * AngleScale = radians
        public static string Order = "XYZ";                       // applied first .. last

        public static float[] Euler(float rx, float ry, float rz)
        {
            float[] m = { 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0 };
            foreach (char c in Order)
            {
                float a = (c == 'X' ? rx : c == 'Y' ? ry : rz) * AngleScale, cs = (float)Math.Cos(a), sn = (float)Math.Sin(a);
                float[] r = c == 'X' ? new float[] { 1, 0, 0, 0, cs, -sn, 0, sn, cs, 0, 0, 0 }
                          : c == 'Y' ? new float[] { cs, 0, sn, 0, 1, 0, -sn, 0, cs, 0, 0, 0 }
                          : new float[] { cs, -sn, 0, sn, cs, 0, 0, 0, 1, 0, 0, 0 };
                m = Mul(r, m);
            }
            return m;
        }

        public static float[][] Globals(Skeleton s, Func<int, float[]> local)
        {
            // Parents usually come first, but not always (some weapon models list a child before
            // its parent): resolve each node's parent chain on demand.
            var g = new float[s.Count][];
            Func<int, float[]> get = null;
            get = i =>
            {
                if (g[i] == null)
                    g[i] = s.Parent[i] < 0 || s.Parent[i] >= s.Count ? local(i) : Mul(get(s.Parent[i]), local(i));
                return g[i];
            };
            for (int i = 0; i < s.Count; i++)
                get(i);
            return g;
        }

        static float[] QuatOf(float[] m)
        {
            float t = m[0] + m[4] + m[8];
            float[] q;
            if (t > 0) { float s = (float)Math.Sqrt(t + 1) * 2; q = new[] { s / 4, (m[7] - m[5]) / s, (m[2] - m[6]) / s, (m[3] - m[1]) / s }; }
            else if (m[0] > m[4] && m[0] > m[8]) { float s = (float)Math.Sqrt(1 + m[0] - m[4] - m[8]) * 2; q = new[] { (m[7] - m[5]) / s, s / 4, (m[1] + m[3]) / s, (m[2] + m[6]) / s }; }
            else if (m[4] > m[8]) { float s = (float)Math.Sqrt(1 + m[4] - m[0] - m[8]) * 2; q = new[] { (m[2] - m[6]) / s, (m[1] + m[3]) / s, s / 4, (m[5] + m[7]) / s }; }
            else { float s = (float)Math.Sqrt(1 + m[8] - m[0] - m[4]) * 2; q = new[] { (m[3] - m[1]) / s, (m[2] + m[6]) / s, (m[5] + m[7]) / s, s / 4 }; }
            return q;
        }

        // The pose a fraction t of the way from clip a's first frame to clip b's, turning each
        // node the short way (mixing Euler angles directly bends some poses out of shape).
        public static Animation MixPoses(Animation a, Animation b, float t)
        {
            int n = Math.Max(a.Nodes.Length, b.Nodes.Length);
            var o = new Animation { Frames = 0, Nodes = new List<AnimChannel>[n] };
            float[] none = { 0, 0, 0 };
            for (int i = 0; i < n; i++)
            {
                o.Nodes[i] = new List<AnimChannel>();
                Func<Animation, int, AnimChannel> chan = (x, kind) => i < x.Nodes.Length ? x.Nodes[i].Find(c => c.Kind == kind) : null;
                AnimChannel ta = chan(a, 1), tb = chan(b, 1), ra = chan(a, 5), rb = chan(b, 5);
                if (ta != null || tb != null)
                {
                    var u = ta != null ? Sample(ta, 0) : Sample(tb, 0);
                    var v = tb != null ? Sample(tb, 0) : u;
                    o.Nodes[i].Add(new AnimChannel { Kind = 1, Frames = new[] { 0 }, Values = new[] { new[] { u[0] + (v[0] - u[0]) * t, u[1] + (v[1] - u[1]) * t, u[2] + (v[2] - u[2]) * t } } });
                }
                if (ra == null && rb == null) continue;
                var ea = ra != null ? Sample(ra, 0) : none;
                var eb = rb != null ? Sample(rb, 0) : none;
                float[] qa = QuatOf(Euler(ea[0], ea[1], ea[2])), qb = QuatOf(Euler(eb[0], eb[1], eb[2]));
                float dot = 0;
                for (int k = 0; k < 4; k++) dot += qa[k] * qb[k];
                if (dot < 0) { dot = -dot; for (int k = 0; k < 4; k++) qb[k] = -qb[k]; }
                float ang = (float)Math.Acos(Math.Min(1f, dot)), sn = (float)Math.Sin(ang);
                float wa = sn < 1e-4f ? 1 - t : (float)Math.Sin((1 - t) * ang) / sn, wb = sn < 1e-4f ? t : (float)Math.Sin(t * ang) / sn;
                float w = wa * qa[0] + wb * qb[0], x = wa * qa[1] + wb * qb[1], y = wa * qa[2] + wb * qb[2], z = wa * qa[3] + wb * qb[3];
                float len = (float)Math.Sqrt(w * w + x * x + y * y + z * z);
                w /= len; x /= len; y /= len; z /= len;
                // Back to Euler angles (rotation matrix Rz * Ry * Rx; see Euler()).
                float m00 = 1 - 2 * (y * y + z * z), m10 = 2 * (x * y + w * z), m20 = 2 * (x * z - w * y), m21 = 2 * (y * z + w * x), m22 = 1 - 2 * (x * x + y * y);
                float rx = (float)Math.Atan2(m21, m22), ry = (float)Math.Asin(Math.Max(-1f, Math.Min(1f, -m20))), rz = (float)Math.Atan2(m10, m00);
                o.Nodes[i].Add(new AnimChannel { Kind = 5, Frames = new[] { 0 }, Values = new[] { new[] { rx / AngleScale, ry / AngleScale, rz / AngleScale } } });
            }
            return o;
        }

        public static float[] PoseLocal(Skeleton s, Animation a, int node, float frame)
        {
            var bind = s.Local[node];
            var m = (float[])bind.Clone();
            if (a == null || node >= a.Nodes.Length) return m;
            foreach (var ch in a.Nodes[node])
            {
                var v = Sample(ch, frame);
                if (ch.Kind == 1) { m[9] = v[0]; m[10] = v[1]; m[11] = v[2]; }
                else if (ch.Kind == 5) { var r = Euler(v[0], v[1], v[2]); Array.Copy(r, 0, m, 0, 9); }
            }
            return m;
        }

        // Returns skinned positions per *position* (not per vertex).
        public static float[] Skin(float[] bindPos, SkinInfluence[] skin, Skeleton s, Animation a, float frame)
        {
            var bindG = Globals(s, i => s.Local[i]);
            var poseG = Globals(s, i => PoseLocal(s, a, i, frame));
            var skinM = new float[s.Count][];
            for (int i = 0; i < s.Count; i++) skinM[i] = Mul(poseG[i], Inverse(bindG[i]));
            var r = new float[bindPos.Length];
            for (int i = 0; i < skin.Length; i++)
            {
                float x = 0, y = 0, z = 0;
                for (int k = 0; k < skin[i].Nodes.Length; k++)
                {
                    float ox, oy, oz;
                    Apply(skinM[skin[i].Nodes[k]], bindPos[i * 3], bindPos[i * 3 + 1], bindPos[i * 3 + 2], out ox, out oy, out oz);
                    x += ox * skin[i].Weights[k]; y += oy * skin[i].Weights[k]; z += oz * skin[i].Weights[k];
                }
                r[i * 3] = x; r[i * 3 + 1] = y; r[i * 3 + 2] = z;
            }
            return r;
        }
    }
}
