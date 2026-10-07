namespace hwk::geo
{
    MeshData sweptRoundedRect (float halfW, float halfD, float radius, int segs,
                               const std::vector<ProfilePoint>& profile, bool capTop)
    {
        MeshData mesh;
        jassert (profile.size() >= 1);

        struct RingPoint { float cx, cz, dx, dz; };
        std::vector<RingPoint> ring;

        const float centres[4][2] { { halfW, halfD }, { -halfW, halfD }, { -halfW, -halfD }, { halfW, -halfD } };

        for (int c = 0; c < 4; ++c)
            for (int s = 0; s <= segs; ++s)
            {
                const float a = ((float) c + (float) s / (float) segs) * 0.5f * kPi;
                ring.push_back ({ centres[c][0], centres[c][1], std::cos (a), std::sin (a) });
            }

        const auto n = ring.size();

        auto pointAt = [&] (const RingPoint& rp, float outset, float y)
        {
            const float r = std::max (0.0f, radius + outset);
            return Vec3 { rp.cx + rp.dx * r, y, rp.cz + rp.dz * r };
        };

        for (size_t k = 0; k + 1 < profile.size(); ++k)
        {
            const auto p0 = profile[k], p1 = profile[k + 1];
            const float dd = p1.outset - p0.outset, dy = p1.y - p0.y;
            const float len = std::max (1.0e-6f, std::sqrt (dd * dd + dy * dy));
            const float nOut = dy / len, nUp = -dd / len;

            const auto base = (juce::uint32) mesh.vertices.size();

            for (auto& rp : ring)
            {
                const Vec3 normal { rp.dx * nOut, nUp, rp.dz * nOut };
                const auto a = pointAt (rp, p0.outset, p0.y);
                const auto b = pointAt (rp, p1.outset, p1.y);
                mesh.addVertex (a, normal, a.x, a.z);
                mesh.addVertex (b, normal, b.x, b.z);
            }

            for (size_t j = 0; j < n; ++j)
            {
                const auto j1 = (j + 1) % n;
                mesh.addQuad (base + (juce::uint32) (j * 2), base + (juce::uint32) (j1 * 2),
                              base + (juce::uint32) (j1 * 2 + 1), base + (juce::uint32) (j * 2 + 1));
            }
        }

        if (capTop)
        {
            const auto last = profile.back();
            const auto centre = mesh.addVertex ({ 0, last.y, 0 }, { 0, 1, 0 }, 0, 0);
            const auto base = (juce::uint32) mesh.vertices.size();

            for (auto& rp : ring)
            {
                const auto p = pointAt (rp, last.outset, last.y);
                mesh.addVertex (p, { 0, 1, 0 }, p.x, p.z);
            }

            for (size_t j = 0; j < n; ++j)
                mesh.addTriangle (centre, base + (juce::uint32) j, base + (juce::uint32) ((j + 1) % n));
        }

        return mesh;
    }

    MeshData lathe (float radius, const std::vector<ProfilePoint>& profile, int segments, bool capTop, const Relief& relief)
    {
        MeshData mesh;
        jassert (profile.size() >= 1 && segments >= 3);

        // How much of the relief applies at height y (eased at the ends of its band)
        auto band = [&] (float y)
        {
            if (relief.count <= 0 || y < relief.yFrom || y > relief.yTo)
                return 0.0f;
            const float e = std::max (1.0e-5f, relief.ease);
            const float t = std::min ({ 1.0f, (y - relief.yFrom) / e, (relief.yTo - y) / e });
            return t * t * (3.0f - 2.0f * t);
        };

        // Groove shape around the circumference, 0 (land) .. 1 (bottom of a groove)
        auto groove = [&] (float angle)
        {
            const float c = 0.5f + 0.5f * std::cos (angle * (float) relief.count);   // 1 at a land
            const float sharp = std::pow (c, 1.0f + 3.0f * relief.sharpness);
            return 1.0f - sharp;
        };

        auto point = [&] (float angle, float outset, float y)
        {
            const float r = std::max (0.0f, (radius + outset) * (1.0f - relief.depth * band (y) * groove (angle)));
            return Vec3 { std::sin (angle) * r, y, -std::cos (angle) * r };   // angle 0 at -z, like a knob's pointer
        };

        const float step = 2.0f * kPi / (float) segments;

        for (size_t k = 0; k + 1 < profile.size(); ++k)
        {
            const auto p0 = profile[k], p1 = profile[k + 1];
            const bool relieved = relief.count > 0 && std::max (p0.y, p1.y) > relief.yFrom && std::min (p0.y, p1.y) < relief.yTo;
            const int rows = relieved ? 4 : 1;
            const auto base = (juce::uint32) mesh.vertices.size();

            for (int j = 0; j <= segments; ++j)
            {
                const float a = step * (float) j;
                for (int r = 0; r <= rows; ++r)
                {
                    const float t = (float) r / (float) rows;
                    const float outset = p0.outset + (p1.outset - p0.outset) * t, y = p0.y + (p1.y - p0.y) * t;
                    const auto p = point (a, outset, y);

                    // Normal from the surface's own tangents (around, and along the profile)
                    const float h = 1.0e-3f;
                    const auto da = point (a + h, outset, y) - point (a - h, outset, y);
                    const float dt = 1.0f / (float) rows * 0.25f;
                    const float t0 = std::max (0.0f, t - dt), t1 = std::min (1.0f, t + dt);
                    const auto q0 = point (a, p0.outset + (p1.outset - p0.outset) * t0, p0.y + (p1.y - p0.y) * t0);
                    const auto q1 = point (a, p0.outset + (p1.outset - p0.outset) * t1, p0.y + (p1.y - p0.y) * t1);
                    // Oriented like the profile's own outward normal (as sweptRoundedRect does it)
                    const float dd = p1.outset - p0.outset, dy = p1.y - p0.y;
                    const float len = std::max (1.0e-6f, std::sqrt (dd * dd + dy * dy));
                    const Vec3 radial { std::sin (a), 0.0f, -std::cos (a) };
                    const Vec3 reference = radial * (dy / len) + Vec3 { 0.0f, -dd / len, 0.0f };
                    auto n = gfx::normalise (gfx::cross (q1 - q0, da));
                    if (gfx::dot (n, reference) < 0.0f)
                        n = -n;
                    if (gfx::length (gfx::cross (q1 - q0, da)) < 1.0e-12f)
                        n = reference;
                    mesh.addVertex (p, n, (float) j / (float) segments, y);
                }
            }

            const auto stride = (juce::uint32) (rows + 1);
            for (int j = 0; j < segments; ++j)
                for (int r = 0; r < rows; ++r)
                {
                    const auto i0 = base + (juce::uint32) j * stride + (juce::uint32) r;
                    const auto i1 = base + (juce::uint32) (j + 1) * stride + (juce::uint32) r;
                    mesh.addQuad (i0, i1, i1 + 1, i0 + 1);
                }
        }

        if (capTop)
        {
            const auto last = profile.back();
            const auto centre = mesh.addVertex ({ 0, last.y, 0 }, { 0, 1, 0 }, 0.5f, 0.5f);
            const auto base = (juce::uint32) mesh.vertices.size();
            for (int j = 0; j <= segments; ++j)
            {
                const auto p = point (step * (float) j, last.outset, last.y);
                mesh.addVertex (p, { 0, 1, 0 }, p.x, p.z);
            }
            for (int j = 0; j < segments; ++j)
                mesh.addTriangle (centre, base + (juce::uint32) j, base + (juce::uint32) j + 1);
        }

        return mesh;
    }

    MeshData sweptPolygon (int sides, float radius, const std::vector<ProfilePoint>& profile, bool capTop)
    {
        MeshData mesh;
        const float halfStep = kPi / (float) sides;

        auto corner = [&] (int i, float outset, float y)
        {
            // Profile outset is measured across the flats, so scale it to the corner distance
            const float a = (float) i * 2.0f * halfStep;
            const float r = std::max (0.0f, radius + outset / std::cos (halfStep));
            return Vec3 { std::cos (a) * r, y, std::sin (a) * r };
        };

        for (size_t k = 0; k + 1 < profile.size(); ++k)
        {
            const auto p0 = profile[k], p1 = profile[k + 1];
            const float dd = p1.outset - p0.outset, dy = p1.y - p0.y;
            const float len = std::max (1.0e-6f, std::sqrt (dd * dd + dy * dy));
            const float nOut = dy / len, nUp = -dd / len;

            for (int s = 0; s < sides; ++s)
            {
                const float mid = ((float) s * 2.0f + 1.0f) * halfStep;
                const Vec3 normal { std::cos (mid) * nOut, nUp, std::sin (mid) * nOut };
                const auto a = mesh.addVertex (corner (s, p0.outset, p0.y), normal, 0, 0);
                const auto b = mesh.addVertex (corner (s + 1, p0.outset, p0.y), normal, 1, 0);
                const auto c = mesh.addVertex (corner (s + 1, p1.outset, p1.y), normal, 1, 1);
                const auto d = mesh.addVertex (corner (s, p1.outset, p1.y), normal, 0, 1);
                mesh.addQuad (a, b, c, d);
            }
        }

        if (capTop)
        {
            const auto last = profile.back();
            const auto centre = mesh.addVertex ({ 0, last.y, 0 }, { 0, 1, 0 }, 0, 0);
            const auto base = (juce::uint32) mesh.vertices.size();
            for (int s = 0; s < sides; ++s)
            {
                const auto p = corner (s, last.outset, last.y);
                mesh.addVertex (p, { 0, 1, 0 }, p.x, p.z);
            }
            for (int s = 0; s < sides; ++s)
                mesh.addTriangle (centre, base + (juce::uint32) s, base + (juce::uint32) ((s + 1) % sides));
        }

        return mesh;
    }

    MeshData plateWithHoles (const Rect& outer, float y, const std::vector<Rect>& holes)
    {
        std::vector<float> xs { outer.minX(), outer.maxX() }, zs { outer.minZ(), outer.maxZ() };

        for (auto& h : holes)
        {
            xs.push_back (h.minX()); xs.push_back (h.maxX());
            zs.push_back (h.minZ()); zs.push_back (h.maxZ());
        }

        auto sortUnique = [] (std::vector<float>& v)
        {
            std::sort (v.begin(), v.end());
            v.erase (std::unique (v.begin(), v.end(), [] (float a, float b) { return std::abs (a - b) < 1.0e-5f; }), v.end());
        };

        sortUnique (xs);
        sortUnique (zs);

        MeshData mesh;

        auto emitQuad = [&] (float x0, float x1, float z0, float z1)
        {
            const Vec3 up { 0, 1, 0 };
            const auto a = mesh.addVertex ({ x0, y, z0 }, up, x0, z0);
            const auto b = mesh.addVertex ({ x1, y, z0 }, up, x1, z0);
            const auto c = mesh.addVertex ({ x1, y, z1 }, up, x1, z1);
            const auto d = mesh.addVertex ({ x0, y, z1 }, up, x0, z1);
            mesh.addQuad (a, d, c, b);
        };

        for (size_t zi = 0; zi + 1 < zs.size(); ++zi)
        {
            const float z0 = zs[zi], z1 = zs[zi + 1], zc = 0.5f * (z0 + z1);
            int spanStart = -1;

            for (size_t xi = 0; xi + 1 < xs.size(); ++xi)
            {
                const float xc = 0.5f * (xs[xi] + xs[xi + 1]);
                bool inside = false;

                for (auto& h : holes)
                    if (xc > h.minX() && xc < h.maxX() && zc > h.minZ() && zc < h.maxZ())
                        inside = true;

                if (! inside && spanStart < 0)
                    spanStart = (int) xi;

                if (inside && spanStart >= 0)
                {
                    emitQuad (xs[(size_t) spanStart], xs[xi], z0, z1);
                    spanStart = -1;
                }
            }

            if (spanStart >= 0)
                emitQuad (xs[(size_t) spanStart], xs.back(), z0, z1);
        }

        return mesh;
    }

    MeshData wellWalls (const Rect& h, float topY, float depth)
    {
        MeshData mesh;
        const float y0 = topY - depth, y1 = topY;

        auto wall = [&] (Vec3 a, Vec3 b, Vec3 normal)
        {
            const auto i0 = mesh.addVertex ({ a.x, y0, a.z }, normal, 0, 0);
            const auto i1 = mesh.addVertex ({ b.x, y0, b.z }, normal, 1, 0);
            const auto i2 = mesh.addVertex ({ b.x, y1, b.z }, normal, 1, 1);
            const auto i3 = mesh.addVertex ({ a.x, y1, a.z }, normal, 0, 1);
            mesh.addQuad (i0, i1, i2, i3);
        };

        wall ({ h.minX(), 0, h.minZ() }, { h.maxX(), 0, h.minZ() }, { 0, 0, 1 });
        wall ({ h.maxX(), 0, h.maxZ() }, { h.minX(), 0, h.maxZ() }, { 0, 0, -1 });
        wall ({ h.minX(), 0, h.maxZ() }, { h.minX(), 0, h.minZ() }, { 1, 0, 0 });
        wall ({ h.maxX(), 0, h.minZ() }, { h.maxX(), 0, h.maxZ() }, { -1, 0, 0 });
        return mesh;
    }

    MeshData horizontalQuad (const Rect& r, float y)
    {
        MeshData mesh;
        const Vec3 up { 0, 1, 0 };
        const auto a = mesh.addVertex ({ r.minX(), y, r.minZ() }, up, 0, 0);
        const auto b = mesh.addVertex ({ r.maxX(), y, r.minZ() }, up, 1, 0);
        const auto c = mesh.addVertex ({ r.maxX(), y, r.maxZ() }, up, 1, 1);
        const auto d = mesh.addVertex ({ r.minX(), y, r.maxZ() }, up, 0, 1);
        mesh.addQuad (a, d, c, b);
        return mesh;
    }

    MeshData quad (Vec3 a, Vec3 b, Vec3 c, Vec3 d)
    {
        MeshData mesh;
        const auto n = gfx::normalise (gfx::cross (b - a, d - a));
        const auto i0 = mesh.addVertex (a, n, 0.0f, 0.0f);
        const auto i1 = mesh.addVertex (b, n, 1.0f, 0.0f);
        const auto i2 = mesh.addVertex (c, n, 1.0f, 1.0f);
        const auto i3 = mesh.addVertex (d, n, 0.0f, 1.0f);
        mesh.addQuad (i0, i1, i2, i3);
        return mesh;
    }

    MeshData box (Vec3 lo, Vec3 hi)
    {
        MeshData mesh;

        auto face = [&] (Vec3 n, Vec3 a, Vec3 b, Vec3 c, Vec3 d)
        {
            const auto i0 = mesh.addVertex (a, n, 0, 0);
            const auto i1 = mesh.addVertex (b, n, 1, 0);
            const auto i2 = mesh.addVertex (c, n, 1, 1);
            const auto i3 = mesh.addVertex (d, n, 0, 1);
            mesh.addQuad (i0, i1, i2, i3);
        };

        face ({ 0, 1, 0 },  { lo.x, hi.y, lo.z }, { hi.x, hi.y, lo.z }, { hi.x, hi.y, hi.z }, { lo.x, hi.y, hi.z });
        face ({ 0, -1, 0 }, { lo.x, lo.y, lo.z }, { hi.x, lo.y, lo.z }, { hi.x, lo.y, hi.z }, { lo.x, lo.y, hi.z });
        face ({ 0, 0, 1 },  { lo.x, lo.y, hi.z }, { hi.x, lo.y, hi.z }, { hi.x, hi.y, hi.z }, { lo.x, hi.y, hi.z });
        face ({ 0, 0, -1 }, { lo.x, lo.y, lo.z }, { hi.x, lo.y, lo.z }, { hi.x, hi.y, lo.z }, { lo.x, hi.y, lo.z });
        face ({ 1, 0, 0 },  { hi.x, lo.y, lo.z }, { hi.x, lo.y, hi.z }, { hi.x, hi.y, hi.z }, { hi.x, hi.y, lo.z });
        face ({ -1, 0, 0 }, { lo.x, lo.y, lo.z }, { lo.x, lo.y, hi.z }, { lo.x, hi.y, hi.z }, { lo.x, hi.y, lo.z });
        return mesh;
    }

    MeshData flatAnnulus (float r0, float r1, int segments)
    {
        MeshData mesh;
        for (int s = 0; s <= segments; ++s)
        {
            const float a = 2.0f * kPi * (float) s / (float) segments;
            const float c = std::cos (a), sn = std::sin (a);
            mesh.addVertex ({ c * r0, 0, sn * r0 }, { 0, 1, 0 }, c * r0, sn * r0);
            mesh.addVertex ({ c * r1, 0, sn * r1 }, { 0, 1, 0 }, c * r1, sn * r1);
        }
        for (juce::uint32 s = 0; s < (juce::uint32) segments; ++s)
            mesh.addQuad (s * 2, s * 2 + 2, s * 2 + 3, s * 2 + 1);
        return mesh;
    }

    MeshData dome (float radius, float height, int segments, int rings)
    {
        MeshData mesh;

        for (int r = 0; r <= rings; ++r)
        {
            const float phi = 0.5f * kPi * (float) r / (float) rings;
            const float ringR = radius * std::cos (phi), y = height * std::sin (phi);

            for (int s = 0; s <= segments; ++s)
            {
                const float a = 2.0f * kPi * (float) s / (float) segments;
                const Vec3 n = gfx::normalise ({ std::cos (phi) * std::cos (a) / radius, std::sin (phi) / height, std::cos (phi) * std::sin (a) / radius });
                mesh.addVertex ({ ringR * std::cos (a), y, ringR * std::sin (a) }, n, (float) s / (float) segments, (float) r / (float) rings);
            }
        }

        const auto stride = (juce::uint32) segments + 1;
        for (juce::uint32 r = 0; r < (juce::uint32) rings; ++r)
            for (juce::uint32 s = 0; s < (juce::uint32) segments; ++s)
                mesh.addQuad (r * stride + s, r * stride + s + 1, (r + 1) * stride + s + 1, (r + 1) * stride + s);

        return mesh;
    }

    MeshData triangularBlade (float b, float h, float t)
    {
        MeshData mesh;
        const float tip = 0.012f, base = -0.02f;

        // Outline in (x, y), counter-clockwise; extruded along z by ±t
        const std::array<std::pair<float, float>, 4> outline {{ { -b, base }, { b, base }, { tip, h }, { -tip, h } }};

        for (float side : { 1.0f, -1.0f })
        {
            const Vec3 n { 0, 0, side };
            std::array<juce::uint32, 4> idx {};
            for (size_t i = 0; i < 4; ++i)
                idx[i] = mesh.addVertex ({ outline[i].first, outline[i].second, side * t }, n, outline[i].first, outline[i].second);
            mesh.addQuad (idx[0], idx[1], idx[2], idx[3]);
        }

        for (size_t i = 0; i < 4; ++i)
        {
            const auto p0 = outline[i], p1 = outline[(i + 1) % 4];
            const float dx = p1.first - p0.first, dy = p1.second - p0.second;
            const Vec3 n = gfx::normalise ({ dy, -dx, 0 });

            const auto a = mesh.addVertex ({ p0.first, p0.second, -t }, n, 0, 0);
            const auto bb = mesh.addVertex ({ p0.first, p0.second, t }, n, 1, 0);
            const auto c = mesh.addVertex ({ p1.first, p1.second, t }, n, 1, 1);
            const auto d = mesh.addVertex ({ p1.first, p1.second, -t }, n, 0, 1);
            mesh.addQuad (a, bb, c, d);
        }

        return mesh;
    }


    MeshData pointerPlate (float length, float tail, float halfWidth, float y0, float y1)
    {
        // Outline (x, z), counter-clockwise when seen from +y: a rounded tail and a pointed tip at -z
        std::vector<std::pair<float, float>> outline;
        constexpr int arc = 10;
        for (int i = 0; i <= arc; ++i)
        {
            const float a = kPi * (float) i / (float) arc;                 // 0..pi around the tail (+z side)
            outline.push_back ({ halfWidth * std::cos (a), tail + halfWidth * std::sin (a) * 0.9f });
        }
        outline.push_back ({ -halfWidth * 0.55f, -length * 0.55f });
        outline.push_back ({ 0.0f, -length });
        outline.push_back ({ halfWidth * 0.55f, -length * 0.55f });

        MeshData mesh;
        const auto n = outline.size();

        // Top cap (fan around the centroid)
        const auto centre = mesh.addVertex ({ 0.0f, y1, 0.0f }, { 0, 1, 0 }, 0.5f, 0.5f);
        const auto top = (juce::uint32) mesh.vertices.size();
        for (auto& p : outline)
            mesh.addVertex ({ p.first, y1, p.second }, { 0, 1, 0 }, p.first, p.second);
        for (size_t i = 0; i < n; ++i)
            mesh.addTriangle (centre, top + (juce::uint32) ((i + 1) % n), top + (juce::uint32) i);

        // Walls, each with its own normal
        for (size_t i = 0; i < n; ++i)
        {
            const auto p0 = outline[i], p1 = outline[(i + 1) % n];
            const Vec3 normal = gfx::normalise ({ p1.second - p0.second, 0.0f, -(p1.first - p0.first) });
            const auto a = mesh.addVertex ({ p0.first, y0, p0.second }, normal, 0, 0);
            const auto b = mesh.addVertex ({ p1.first, y0, p1.second }, normal, 1, 0);
            const auto c = mesh.addVertex ({ p1.first, y1, p1.second }, normal, 1, 1);
            const auto d = mesh.addVertex ({ p0.first, y1, p0.second }, normal, 0, 1);
            mesh.addQuad (a, d, c, b);
        }
        return mesh;
    }

    MeshData unitQuad()
    {
        return horizontalQuad ({ 0.0f, 0.0f, 1.0f, 1.0f }, 0.0f);
    }
}
