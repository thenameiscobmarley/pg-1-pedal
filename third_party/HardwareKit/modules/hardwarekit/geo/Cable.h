#pragma once

/*  The cable sculptor: a cable that finds its own shape, as a real one does, instead of being threaded
    through hand-placed points (which kinked, twisted and cut through things whenever anything moved).

    A cable is a rope of fixed length: `nodes` beads held apart by their segment's length, pulled down by
    gravity, stiff against bending (it never turns tighter than `minBendRadius` - no kinks), lying on the
    floor rather than through it, pushed out of the boxes it must not pass through, and leaving each plug
    straight along the plug's axis for a strain relief's length. It settles by position-based dynamics
    (Verlet steps, then the constraints relaxed a few times each step) - a few thousand small steps once,
    when the scene is built: nothing is spent per frame.

    tube() then sweeps a round section along the settled path with rotation-minimising frames (the
    double-reflection method, Wang et al., 2008), so the tube never twists or flips round a bend, and
    its texture's seam runs straight down the cable.

      auto path = hwk::cable::sculpt ({ plugTip, plugAxis }, { socketTip, socketAxis }, spec, obstacles);
      auto mesh = hwk::cable::tube (path, spec.radius, 12);
*/
namespace hwk::cable
{
    using gfx::Vec3;
    using gfx::MeshData;

    /** One end: where the cable leaves its plug, and which way (a unit vector out of the plug). */
    struct End { Vec3 at, out; };

    /** A box it must not pass through (world, axis-aligned). */
    struct Box { Vec3 lo, hi; };

    struct Spec
    {
        float radius = 0.03f;
        float slack = 1.12f;          // its length against the plugs' straight distance (1: taut)
        float extraLength = 0.0f;     // and this much more (a cable longer than it needs)
        float minBendRadius = 0.12f;  // it never bends tighter than this
        float strainRelief = 0.08f;   // straight out of each plug for this long
        float floorY = -1.0e9f;       // the surface it lies on (its underside rests on it)
        float stiffness = 0.35f;      // 0 .. 1: how much it resists every bend, not only tight ones
        Vec3 gravity { 0.0f, -9.8f, 0.0f };
        int nodes = 48, steps = 320, relax = 6;
    };

    namespace detail
    {
        inline Vec3 lerp (Vec3 a, Vec3 b, float t) noexcept { return a + (b - a) * t; }
        /** Points spaced evenly along a polyline, n of them. */
        inline std::vector<Vec3> resample (const std::vector<Vec3>& poly, int n)
        {
            std::vector<float> acc (poly.size(), 0.0f);
            for (size_t i = 1; i < poly.size(); ++i) acc[i] = acc[i - 1] + gfx::length (poly[i] - poly[i - 1]);
            const float total = std::max (1.0e-6f, acc.back());
            std::vector<Vec3> out ((size_t) n);
            size_t seg = 1;
            for (int k = 0; k < n; ++k)
            {
                const float d = total * (float) k / (float) (n - 1);
                while (seg + 1 < poly.size() && acc[seg] < d) ++seg;
                const float span = std::max (1.0e-6f, acc[seg] - acc[seg - 1]);
                out[(size_t) k] = lerp (poly[seg - 1], poly[seg], std::clamp ((d - acc[seg - 1]) / span, 0.0f, 1.0f));
            }
            return out;
        }
    }

    /** The cable's resting path from end a to end b: `spec.nodes` points. */
    inline std::vector<Vec3> sculpt (End a, End b, const Spec& spec, const std::vector<Box>& obstacles = {})
    {
        using gfx::normalise; using gfx::length; using gfx::dot;
        a.out = normalise (a.out); b.out = normalise (b.out);
        const int n = std::max (8, spec.nodes);
        const float direct = length (b.at - a.at);
        const float relief = spec.strainRelief;
        const float total = std::max (direct * spec.slack + spec.extraLength, direct + 2.0f * relief + 0.01f);
        const float seg = total / (float) (n - 1);

        // Start: out of each plug, then the two ends joined, the whole length laid along it (slack as a droop)
        const Vec3 a1 = a.at + a.out * (relief + 0.25f * direct), b1 = b.at + b.out * (relief + 0.25f * direct);
        const float droop = std::sqrt (std::max (0.0f, total * total - direct * direct)) * 0.45f;
        std::vector<Vec3> poly { a.at, a.at + a.out * relief, a1, (a1 + b1) * 0.5f + normalise (spec.gravity) * droop, b1, b.at + b.out * relief, b.at };
        auto p = detail::resample (poly, n);
        auto prev = p;

        // How many beads each strain relief holds straight
        const int hold = std::clamp ((int) std::ceil (relief / seg), 1, n / 4);
        const float minChord = 2.0f * seg * std::cos (std::min (1.2f, 0.5f * seg / std::max (1.0e-4f, spec.minBendRadius)));   // (neighbours' distance at the tightest bend allowed)
        const float dt = 1.0f / 120.0f, damping = 0.96f;
        const float floorAt = spec.floorY + spec.radius;

        auto pinEnds = [&]
        {
            for (int k = 0; k <= hold; ++k)
            {
                p[(size_t) k] = a.at + a.out * (seg * (float) k);
                p[(size_t) (n - 1 - k)] = b.at + b.out * (seg * (float) k);
            }
        };
        for (int step = 0; step < spec.steps; ++step)
        {
            // Verlet: carry on moving (damped), and fall
            for (int i = hold + 1; i < n - 1 - hold; ++i)
            {
                const Vec3 v = (p[(size_t) i] - prev[(size_t) i]) * damping;
                prev[(size_t) i] = p[(size_t) i];
                p[(size_t) i] = p[(size_t) i] + v + spec.gravity * (dt * dt);
            }
            for (int it = 0; it < spec.relax; ++it)
            {
                pinEnds();
                // length: each segment back to its length (both ends moving, but a held bead does not)
                for (int i = 0; i + 1 < n; ++i)
                {
                    const Vec3 d = p[(size_t) i + 1] - p[(size_t) i];
                    const float l = std::max (1.0e-6f, length (d));
                    const Vec3 fix = d * (0.5f * (l - seg) / l);
                    const bool fa = i <= hold || i >= n - 1 - hold, fb = i + 1 <= hold || i + 1 >= n - 1 - hold;
                    if (! fa) p[(size_t) i] = p[(size_t) i] + (fb ? fix * 2.0f : fix);
                    if (! fb) p[(size_t) i + 1] = p[(size_t) i + 1] - (fa ? fix * 2.0f : fix);
                }
                // bending: never tighter than the minimum radius, and a general stiffness (toward the neighbours' mid)
                for (int i = hold + 1; i < n - 1 - hold; ++i)
                {
                    const Vec3 lo = p[(size_t) i - 1], hi = p[(size_t) i + 1];
                    const Vec3 mid = (lo + hi) * 0.5f;
                    const float chord = length (hi - lo);
                    const float k = chord < minChord ? 0.9f : spec.stiffness * 0.25f;
                    p[(size_t) i] = detail::lerp (p[(size_t) i], mid, k);
                    if (chord < minChord && chord > 1.0e-6f)
                    {
                        const Vec3 push = (hi - lo) * (0.25f * (minChord - chord) / chord);
                        if (i - 1 > hold) p[(size_t) i - 1] = p[(size_t) i - 1] - push;
                        if (i + 1 < n - 1 - hold) p[(size_t) i + 1] = p[(size_t) i + 1] + push;
                    }
                }
                // collisions: the floor, then the boxes (out along the shortest way)
                for (int i = hold + 1; i < n - 1 - hold; ++i)
                {
                    auto& q = p[(size_t) i];
                    if (q.y < floorAt) q.y = floorAt;
                    for (const auto& bx : obstacles)
                    {
                        const Vec3 lo = bx.lo - Vec3 { spec.radius, spec.radius, spec.radius }, hi = bx.hi + Vec3 { spec.radius, spec.radius, spec.radius };
                        if (q.x > lo.x && q.x < hi.x && q.y > lo.y && q.y < hi.y && q.z > lo.z && q.z < hi.z)
                        {
                            const float dx0 = q.x - lo.x, dx1 = hi.x - q.x, dy0 = q.y - lo.y, dy1 = hi.y - q.y, dz0 = q.z - lo.z, dz1 = hi.z - q.z;
                            const float m = std::min ({ dx0, dx1, dy0, dy1, dz0, dz1 });
                            if (m == dx0) q.x = lo.x; else if (m == dx1) q.x = hi.x;
                            else if (m == dy0) q.y = lo.y; else if (m == dy1) q.y = hi.y;
                            else if (m == dz0) q.z = lo.z; else q.z = hi.z;
                        }
                    }
                }
            }
        }
        pinEnds();
        return p;
    }

    /** A round tube along `path` (a smooth curve through it), `sides` round, with rotation-minimising frames. */
    inline MeshData tube (const std::vector<Vec3>& path, float radius, int sides)
    {
        using gfx::normalise; using gfx::dot; using gfx::cross; using gfx::length;
        MeshData mesh;
        if (path.size() < 2) return mesh;
        // A smooth curve through the beads (Catmull-Rom), about a radius apart
        std::vector<Vec3> c;
        auto at = [&] (int i) { return path[(size_t) std::clamp (i, 0, (int) path.size() - 1)]; };
        for (int i = 0; i + 1 < (int) path.size(); ++i)
        {
            const Vec3 p0 = at (i - 1), p1 = at (i), p2 = at (i + 1), p3 = at (i + 2);
            const int steps = std::max (1, (int) std::ceil (length (p2 - p1) / std::max (0.01f, radius * 0.9f)));
            for (int k = 0; k < steps; ++k)
            {
                const float t = (float) k / (float) steps, t2 = t * t, t3 = t2 * t;
                c.push_back ((p1 * 2.0f + (p2 - p0) * t + (p0 * 2.0f - p1 * 5.0f + p2 * 4.0f - p3) * t2 + (p1 * 3.0f - p0 - p2 * 3.0f + p3) * t3) * 0.5f);
            }
        }
        c.push_back (path.back());
        // Tangents, then frames by double reflection (no twist, no flip)
        std::vector<Vec3> t (c.size());
        for (size_t i = 0; i < c.size(); ++i) t[i] = normalise (i + 1 < c.size() ? c[i + 1] - (i > 0 ? c[i - 1] : c[i]) : c[i] - c[i - 1]);
        Vec3 r = std::abs (t[0].y) < 0.9f ? normalise (cross (t[0], { 0.0f, 1.0f, 0.0f })) : normalise (cross (t[0], { 1.0f, 0.0f, 0.0f }));
        float along = 0.0f;
        const float circumference = 2.0f * 3.14159265f * radius;
        for (size_t i = 0; i < c.size(); ++i)
        {
            if (i > 0)
            {
                const Vec3 v1 = c[i] - c[i - 1];
                const float c1 = dot (v1, v1);
                if (c1 > 1.0e-12f)
                {
                    const Vec3 rL = r - v1 * (2.0f / c1 * dot (v1, r)), tL = t[i - 1] - v1 * (2.0f / c1 * dot (v1, t[i - 1]));
                    const Vec3 v2 = t[i] - tL;
                    const float c2 = dot (v2, v2);
                    r = c2 > 1.0e-12f ? rL - v2 * (2.0f / c2 * dot (v2, rL)) : rL;
                }
                along += length (v1);
            }
            r = normalise (r - t[i] * dot (r, t[i]));
            const Vec3 s = cross (t[i], r);
            for (int k = 0; k <= sides; ++k)
            {
                const float a = 2.0f * 3.14159265f * (float) k / (float) sides;
                const Vec3 d = r * std::cos (a) + s * std::sin (a);
                mesh.addVertex (c[i] + d * radius, d, (float) k / (float) sides, along / circumference);
            }
        }
        const int ring = sides + 1;
        for (int i = 0; i + 1 < (int) c.size(); ++i)
            for (int k = 0; k < sides; ++k)
            {
                const auto a0 = (juce::uint32) (i * ring + k), a1 = (juce::uint32) (i * ring + k + 1);
                const auto b0 = (juce::uint32) ((i + 1) * ring + k), b1 = (juce::uint32) ((i + 1) * ring + k + 1);
                mesh.addQuad (a0, b0, b1, a1);
            }
        return mesh;
    }
}
