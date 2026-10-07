namespace hwk::models
{
    using geo::ProfilePoint;
    using geo::sweptRoundedRect;
    using gfx::Mat4;

    //==============================================================================
    namespace
    {
        /** One recipe style: skirt, body, grip, cap, pointer - everything in multiples of the knob radius
            r unless marked as an absolute size. */
        struct KnobRecipe
        {
            const char* name;
            enum Shape { cylinder, taper, cone, dome, barrel, collet, stepped, disc } shape;
            float height;                    // body height (x r)
            float topR;                      // taper / cone: top radius (x r)
            Vec3 bodyColour; bool bodyMetal; float polish; bool brushed;
            int ribs; float ribDepth; float ribSharp;                 // grip carving (0 ribs = smooth)
            float skirtR; Vec3 skirtColour; bool skirtMetal; int skirtKnurl;   // skirt (0 = none)
            float capR; int capMaterial; Vec3 capColour; bool capDome;        // cap: 0 plastic, 1 metal, 2 unit accent (anodised), 3 fixed colour metal
            enum Pointer { line, dot, notch, bar, skirtLine, wing, sideLine } pointer;
            Vec3 pointerColour;
            float gloss = 1.0f;              // body: 1 glossy .. 0 matte
            float collarR = 0.0f;            // a metal collar under the body (x r, 0 = none), its height x r
            float collarH = 0.0f;
            bool setScrew = false;           // a grub screw in the collar's side
        };

        constexpr Vec3 black   { 0.030f, 0.030f, 0.033f }, darkGrey { 0.10f, 0.10f, 0.11f }, grey { 0.30f, 0.31f, 0.33f };
        constexpr Vec3 cream   { 0.80f, 0.76f, 0.66f }, bakelite { 0.11f, 0.058f, 0.032f }, rubber { 0.075f, 0.075f, 0.08f };
        constexpr Vec3 alu     { 0.74f, 0.74f, 0.76f }, nickel { 0.66f, 0.66f, 0.68f }, gunmetal { 0.36f, 0.37f, 0.40f };
        constexpr Vec3 brass   { 0.78f, 0.62f, 0.34f }, anodBlack { 0.085f, 0.085f, 0.095f }, chromeC { 0.76f, 0.76f, 0.78f };
        // Cap colours as real console / outboard caps come: moulded, muted, never candy-bright
        constexpr Vec3 capRed  { 0.46f, 0.11f, 0.09f }, capBlue { 0.16f, 0.25f, 0.44f }, capGreen { 0.16f, 0.32f, 0.22f };
        constexpr Vec3 capYellow { 0.70f, 0.56f, 0.20f }, capWhite { 0.84f, 0.83f, 0.80f }, amber { 0.46f, 0.29f, 0.11f };
        constexpr Vec3 oxblood { 0.36f, 0.08f, 0.10f };
        constexpr Vec3 white { 0.95f, 0.95f, 0.97f }, ink { 0.02f, 0.02f, 0.02f };
        constexpr Vec3 none {};

        using R = KnobRecipe;
        const KnobRecipe recipes[] {
            // Console: grey collet bodies with coloured caps
            { "Console, unit colour", R::cylinder, 1.05f, 1, grey, false, 0, false, 36, 0.020f, 0.30f, 0, none, false, 0, 0.50f, 2, none, false, R::line, white },
            { "Console, red cap",     R::cylinder, 1.05f, 1, grey, false, 0, false, 36, 0.020f, 0.30f, 0, none, false, 0, 0.50f, 3, capRed, false, R::line, white },
            { "Console, blue cap",    R::cylinder, 1.05f, 1, grey, false, 0, false, 36, 0.020f, 0.30f, 0, none, false, 0, 0.50f, 3, capBlue, false, R::line, white },
            { "Console, green cap",   R::cylinder, 1.05f, 1, grey, false, 0, false, 36, 0.020f, 0.30f, 0, none, false, 0, 0.50f, 3, capGreen, false, R::line, white },
            { "Console, yellow cap",  R::cylinder, 1.05f, 1, grey, false, 0, false, 36, 0.020f, 0.30f, 0, none, false, 0, 0.50f, 3, capYellow, false, R::line, ink },
            { "Console, white cap",   R::cylinder, 1.05f, 1, darkGrey, false, 0, false, 36, 0.020f, 0.30f, 0, none, false, 0, 0.50f, 3, capWhite, false, R::line, ink },
            { "Console, low",         R::disc,     0.55f, 1, grey, false, 0, false, 48, 0.018f, 0.50f, 0, none, false, 0, 0.55f, 2, none, false, R::dot, white },

            // Vintage
            { "Marconi stepped",      R::stepped,  1.10f, 1, black, false, 0, false, 0, 0, 0, 1.30f, alu, true, 0, 0.0f, 0, none, false, R::skirtLine, white },
            { "Bakelite pointer",     R::cylinder, 0.85f, 1, bakelite, false, 0, false, 0, 0, 0, 0, none, false, 0, 0.0f, 0, none, false, R::bar, cream },
            { "Cream radio",          R::dome,     0.95f, 1, cream, false, 0, false, 24, 0.045f, 0.10f, 0, none, false, 0, 0.0f, 0, none, false, R::dot, bakelite },
            { "Bakelite fluted",      R::taper,    1.15f, 0.80f, bakelite, false, 0, false, 20, 0.070f, 0.10f, 0, none, false, 0, 0.0f, 0, none, false, R::line, cream },
            { "Cone on chrome skirt", R::cone,     1.05f, 0.45f, black, false, 0, false, 0, 0, 0, 1.40f, chromeC, true, 0, 0.0f, 0, none, false, R::skirtLine, white },
            { "Wing pointer",         R::dome,     0.80f, 1, black, false, 0, false, 0, 0, 0, 0, none, false, 0, 0.0f, 0, none, false, R::wing, white },
            { "Broadcast, brass cap", R::barrel,   1.10f, 1, black, false, 0, false, 30, 0.030f, 0.40f, 0, none, false, 0, 0.62f, 3, brass, false, R::line, ink },
            { "Davies small",         R::taper,    1.20f, 0.82f, black, false, 0, false, 16, 0.080f, 0.12f, 1.25f, black, false, 0, 0.0f, 0, none, false, R::line, white },

            // Machined metal
            { "Machined silver",      R::cylinder, 0.95f, 1, alu, true, 0.45f, true, 72, 0.022f, 0.90f, 0, none, false, 0, 0.0f, 0, none, false, R::notch, ink },
            { "Machined black",       R::cylinder, 0.95f, 1, anodBlack, true, 0.40f, true, 72, 0.022f, 0.90f, 0, none, false, 0, 0.0f, 0, none, false, R::line, white },
            { "Machined gunmetal",    R::cylinder, 0.95f, 1, gunmetal, true, 0.45f, true, 60, 0.022f, 0.90f, 0, none, false, 0, 0.55f, 2, none, false, R::line, white },
            { "Brass knurl",          R::cylinder, 0.90f, 1, brass, true, 0.55f, true, 64, 0.024f, 0.90f, 0, none, false, 0, 0.0f, 0, none, false, R::dot, ink },
            { "Crosshatch silver",    R::barrel,   1.00f, 1, nickel, true, 0.50f, true, 96, 0.018f, 1.00f, 0, none, false, 0, 0.0f, 0, none, false, R::notch, ink },
            { "Stepped aluminium",    R::stepped,  1.05f, 1, alu, true, 0.50f, true, 0, 0, 0, 0, none, false, 0, 0.0f, 0, none, false, R::line, ink },
            { "Chrome dome",          R::dome,     0.90f, 1, chromeC, true, 0.90f, false, 0, 0, 0, 0, none, false, 0, 0.0f, 0, none, false, R::notch, ink },

            // Instrument collets and Eurorack
            { "Collet, black",        R::collet,   1.30f, 1, black, false, 0, false, 24, 0.030f, 0.30f, 0, none, false, 0, 0.46f, 2, none, false, R::line, white },
            { "Collet, grey",         R::collet,   1.30f, 1, grey, false, 0, false, 24, 0.030f, 0.30f, 0, none, false, 0, 0.46f, 3, capRed, false, R::line, white },
            { "Collet, cream",        R::collet,   1.30f, 1, cream, false, 0, false, 24, 0.030f, 0.30f, 0, none, false, 0, 0.46f, 3, capBlue, false, R::line, white },
            { "Rogan",                R::taper,    1.10f, 0.72f, black, false, 0, false, 32, 0.035f, 0.35f, 0, none, false, 0, 0.0f, 0, none, false, R::line, white },
            { "Rogan with skirt",     R::taper,    1.10f, 0.72f, black, false, 0, false, 32, 0.035f, 0.35f, 1.28f, black, false, 0, 0.0f, 0, none, false, R::skirtLine, white },
            { "Rubber, tall",         R::taper,    1.25f, 0.85f, rubber, false, 0, false, 60, 0.015f, 0.40f, 0, none, false, 0, 0.0f, 0, none, false, R::line, white },
            { "Rubber, low",          R::disc,     0.60f, 1, rubber, false, 0, false, 60, 0.015f, 0.40f, 0, none, false, 0, 0.0f, 0, none, false, R::dot, white },
            { "Rubber, ribbed",       R::barrel,   1.00f, 1, rubber, false, 0, false, 18, 0.060f, 0.20f, 0, none, false, 0, 0.0f, 0, none, false, R::line, white },
            { "Pointer bar, black",   R::cylinder, 0.80f, 1, black, false, 0, false, 0, 0, 0, 0, none, false, 0, 0.0f, 0, none, false, R::bar, white },
            { "Pointer bar, silver",  R::cylinder, 0.80f, 1, alu, true, 0.55f, true, 0, 0, 0, 0, none, false, 0, 0.0f, 0, none, false, R::bar, ink },
            { "Mil-spec pointer",     R::cylinder, 0.85f, 1, anodBlack, true, 0.30f, false, 0, 0, 0, 1.20f, nickel, true, 36, 0.0f, 0, none, false, R::bar, white },

            // Hi-fi
            { "Hi-fi disc",           R::disc,     0.50f, 1, alu, true, 0.55f, true, 0, 0, 0, 0, none, false, 0, 0.0f, 0, none, false, R::dot, ink },
            { "Hi-fi black disc",     R::disc,     0.50f, 1, anodBlack, true, 0.45f, true, 90, 0.012f, 1.00f, 0, none, false, 0, 0.0f, 0, none, false, R::line, white },
            { "Hi-fi dimple",         R::disc,     0.55f, 1, nickel, true, 0.60f, true, 0, 0, 0, 0, none, false, 0, 0.42f, 1, gunmetal, false, R::dot, ink },
            { "Gunmetal, anodised cap", R::taper,  1.00f, 0.90f, gunmetal, true, 0.40f, true, 0, 0, 0, 0, none, false, 0, 0.70f, 2, none, false, R::line, white },

            // Guitar and colour
            { "Top-hat",              R::cylinder, 0.90f, 1, cream, false, 0, false, 30, 0.020f, 0.30f, 1.35f, cream, false, 0, 0.0f, 0, none, false, R::skirtLine, ink },
            { "Speed knob",           R::barrel,   1.15f, 1, black, false, 0, false, 40, 0.025f, 0.60f, 0, none, false, 0, 0.0f, 0, none, false, R::dot, white },
            { "Guitar dome",          R::dome,     1.00f, 1, black, false, 0, false, 28, 0.030f, 0.30f, 0, none, false, 0, 0.48f, 1, chromeC, true, R::line, white },
            { "Amber instrument",     R::cylinder, 1.00f, 1, darkGrey, false, 0, false, 36, 0.022f, 0.30f, 0, none, false, 0, 0.50f, 3, amber, false, R::line, white },
            { "Oxblood instrument",   R::cylinder, 1.00f, 1, darkGrey, false, 0, false, 36, 0.022f, 0.30f, 0, none, false, 0, 0.50f, 3, oxblood, false, R::line, white },

            // Classic studio gear, matched to photographs (ALL KNOBS.png)
            //   name                  shape        h      top    body          metal pol  brush ribs depth  sharp skirtR skirt col  sm  kn  capR mat cap        dome pointer      colour gloss collarR collarH screw
            { "Program EQ top hat",   R::taper,    1.05f, 0.80f, black,        false, 0, false, 11, 0.090f, 0.12f, 1.45f, black, false, 0, 0.0f, 0, none, false, R::sideLine, cream, 1.00f, 0.0f, 0.0f, false },
            { "FET knurled silver",   R::cylinder, 0.72f, 1,     alu,          true, 0.50f, true, 110, 0.018f, 1.00f, 0, none, false, 0, 0.0f, 0, none, false, R::notch, ink, 1.0f, 0.62f, 0.22f, false },
            { "Optical fluted",       R::taper,    1.25f, 0.70f, black,        false, 0, false, 12, 0.075f, 0.18f, 1.36f, black, false, 0, 0.0f, 0, none, false, R::sideLine, white, 1.00f, 0.0f, 0.0f, false },
            { "Small ribbed",         R::cylinder, 1.00f, 1,     rubber,       false, 0, false, 28, 0.026f, 0.55f, 0, none, false, 0, 0.0f, 0, none, false, R::sideLine, white, 0.30f, 0.0f, 0.0f, false },
            { "Channel matte black",  R::barrel,   1.00f, 1,     anodBlack,    false, 0, false, 0, 0, 0, 0, none, false, 0, 0.0f, 0, none, false, R::sideLine, white, 0.15f, 0.72f, 0.20f, false },
            { "Channel red trim",     R::cylinder, 1.00f, 1,     capRed,       true, 0.35f, true, 40, 0.012f, 0.80f, 0, none, false, 0, 0.0f, 0, none, false, R::sideLine, white, 1.0f, 0.72f, 0.20f, false },
            { "Vintage maroon",       R::taper,    0.85f, 0.88f, oxblood,      false, 0, false, 48, 0.012f, 0.60f, 1.35f, oxblood, false, 0, 0.0f, 0, none, false, R::line, white, 0.85f, 0.0f, 0.0f, false },
            { "Vintage dark grey",    R::taper,    0.90f, 0.78f, gunmetal,     false, 0, false, 36, 0.020f, 0.50f, 1.30f, gunmetal, false, 0, 0.0f, 0, none, false, R::line, white, 0.60f, 0.0f, 0.0f, false },
            { "Vintage small grey",   R::taper,    0.95f, 0.80f, grey,         false, 0, false, 24, 0.022f, 0.45f, 1.22f, darkGrey, false, 0, 0.0f, 0, none, false, R::line, white, 0.60f, 0.0f, 0.0f, false },
            { "500 blue cap",         R::cylinder, 1.30f, 1,     darkGrey,     false, 0, false, 40, 0.014f, 0.70f, 0, none, false, 0, 0.92f, 3, capBlue, true, R::line, white, 0.40f, 0.70f, 0.22f, true },
            { "500 red cap",          R::cylinder, 1.30f, 1,     darkGrey,     false, 0, false, 40, 0.014f, 0.70f, 0, none, false, 0, 0.92f, 3, capRed, true, R::line, white, 0.40f, 0.70f, 0.22f, true },
            { "500 white cap",        R::cylinder, 1.30f, 1,     darkGrey,     false, 0, false, 40, 0.014f, 0.70f, 0, none, false, 0, 0.92f, 3, capWhite, true, R::line, ink, 0.40f, 0.70f, 0.22f, true },
            { "Mastering ribbed",     R::taper,    1.00f, 0.84f, black,        false, 0, false, 30, 0.028f, 0.55f, 1.20f, black, false, 0, 0.0f, 0, none, false, R::sideLine, white, 0.55f, 0.0f, 0.0f, false },

            // Made in the Rack Unit Designer's knob maker ("Grey Ribbed", TAKEBACK): a tall dark metal
            // cylinder, 28 shallow ribs, a thin grey metal skirt, a cream line
            { "Grey ribbed metal",    R::cylinder, 1.50f, 0.92f, { 0.22f, 0.22f, 0.22f }, true, 0.40f, false, 28, 0.020f, 0.55f, 1.05f, { 0.39f, 0.39f, 0.41f }, true, 0, 0.0f, 0, none, false, R::line, cream, 1.0f, 0.0f, 0.0f, false },

        };
        static_assert (sizeof (recipes) / sizeof (recipes[0]) == (size_t) KnobStyle::count - (size_t) KnobStyle::consoleAccent,
                       "one recipe per recipe style");

        const KnobRecipe* recipeFor (KnobStyle s) noexcept
        {
            const int i = (int) s - (int) KnobStyle::consoleAccent;
            return i >= 0 && i < (int) (sizeof (recipes) / sizeof (recipes[0])) ? &recipes[i] : nullptr;
        }
    }

    const char* knobStyleName (KnobStyle s) noexcept
    {
        switch (s)
        {
            case KnobStyle::proXl:       return "Pro XL";
            case KnobStyle::fluted:      return "Fluted";
            case KnobStyle::chickenHead: return "Chicken head";
            case KnobStyle::aluminium:   return "Aluminium";
            case KnobStyle::softTouch:   return "Soft touch";
            case KnobStyle::jewelCap:    return "Jewel cap";
            case KnobStyle::skirted:     return "Skirted";
            case KnobStyle::chickenHeadKnob: return "Chicken head, knob";
            default:                     break;
        }
        if (auto* r = recipeFor (s))
            return r->name;
        return "Knob";
    }

    static MeshData pointerLine (float radius, float y, float halfWidth, float inner)
    {
        return geo::box ({ -halfWidth, y - 0.0005f, -(radius - 0.018f) }, { halfWidth, y + 0.003f, -inner });
    }

    int segmentsFor (int detail) noexcept
    {
        constexpr int around[numDetailLevels] { 32, 64, 128, 256 };
        return around[std::clamp (detail, 0, numDetailLevels - 1)];
    }


    /** Builds a recipe style (see the table above). */
    static Model recipeKnob (const KnobRecipe& k, float r, Vec3 accent, int detail)
    {
        using geo::lathe;
        using geo::Relief;
        Model m;
        const int seg = segmentsFor (detail);
        const bool carved = detail >= 2;
        // A metal collar the body sits on (the shaft clamp of machined and modern knobs)
        const float collarH = k.collarR > 0.0f ? k.collarH * r : 0.0f;
        const float skirtH = (k.skirtR > 0.0f ? 0.016f : 0.0f) + collarH;
        const float h = k.height * r;
        const float c = std::min (0.012f, 0.15f * r);
        const float y0 = skirtH;

        // Body profile (outset from r, y)
        std::vector<ProfilePoint> prof;
        float bandFrom = y0 + 0.12f * h, bandTo = y0 + h - c - 0.004f;
        switch (k.shape)
        {
            case KnobRecipe::cylinder: prof = { { 0, y0 }, { 0, y0 + h - c }, { -c * 0.4f, y0 + h - c * 0.35f }, { -c, y0 + h } }; break;
            case KnobRecipe::taper:
            case KnobRecipe::cone:
            {
                const float t = (k.topR - 1.0f) * r;
                prof = { { 0, y0 }, { 0, y0 + 0.08f * h }, { t, y0 + h - c }, { t - c * 0.5f, y0 + h - c * 0.3f }, { t - c, y0 + h } };
                break;
            }
            case KnobRecipe::dome:
            {
                prof = { { 0, y0 }, { 0, y0 + 0.40f * h } };
                for (int i = 1; i <= 8; ++i)
                {
                    const float a = 0.5f * geo::kPi * (float) i / 8.0f;
                    prof.push_back ({ -r * 0.92f * (1.0f - std::cos (a)), y0 + 0.40f * h + 0.60f * h * std::sin (a) });
                }
                bandTo = y0 + 0.40f * h;
                break;
            }
            case KnobRecipe::barrel: prof = { { 0, y0 }, { 0.00f, y0 + 0.12f * h }, { 0.035f * r, y0 + 0.50f * h }, { 0.0f, y0 + 0.88f * h }, { -c, y0 + h } }; break;
            case KnobRecipe::collet:
                prof = { { 0.10f * r, y0 }, { 0.10f * r, y0 + 0.14f * h }, { -0.22f * r, y0 + 0.20f * h }, { -0.22f * r, y0 + h - c }, { -0.22f * r - c, y0 + h } };
                bandFrom = y0 + 0.24f * h;
                break;
            case KnobRecipe::stepped:
                prof = { { 0, y0 }, { 0, y0 + 0.38f * h }, { -0.28f * r, y0 + 0.44f * h }, { -0.28f * r, y0 + h - c }, { -0.28f * r - c, y0 + h } };
                bandFrom = y0 + 0.46f * h;
                break;
            case KnobRecipe::disc: prof = { { 0, y0 }, { 0, y0 + h - c }, { -c, y0 + h } }; break;
        }

        const float topY = prof.back().y;
        const float topR = r + prof.back().outset;

        Relief grip;
        if (carved && k.ribs > 0)
            grip = { k.ribs, k.ribDepth, bandFrom, std::max (bandFrom + 0.005f, bandTo), 0.004f, k.ribSharp };

        if (collarH > 0.0f)
        {
            Part collar { lathe (k.collarR * r, { { 0.0f, 0.0f }, { 0.0f, collarH - 0.002f }, { -0.002f, collarH } }, seg, true),
                          Role::metal, true, { 0.62f, 0.62f, 0.64f } };
            collar.polish = 0.55f;
            m.parts.push_back (std::move (collar));
            if (k.setScrew)   // its grub screw, a dark hex socket in the side
                m.parts.push_back ({ geo::box ({ -0.006f, 0.35f * collarH, k.collarR * r - 0.001f }, { 0.006f, 0.75f * collarH, k.collarR * r + 0.0015f }),
                                     Role::body, true, { 0.02f, 0.02f, 0.02f } });
        }

        if (k.skirtR > 0.0f)
        {
            Relief knurl;
            if (carved && k.skirtKnurl > 0)
                knurl = { k.skirtKnurl, 0.012f, 0.003f, skirtH - 0.003f, 0.002f, 0.9f };
            // Metal skirts stay flat (machined). Moulded ones are shaped like the real thing: a thin rounded
            // rim at the edge, then a smooth slope rising into the body - it catches the light all round
            // instead of reading as a flat washer
            const float w = (k.skirtR - 1.0f) * r;
            const float sb = skirtH - collarH;   // the skirt's own thickness
            std::vector<ProfilePoint> skirtProf = k.skirtMetal
                ? std::vector<ProfilePoint> { { 0.0f, collarH }, { 0.0f, skirtH - 0.004f }, { -0.003f, skirtH }, { -w, skirtH } }
                : std::vector<ProfilePoint> { { 0.0f, collarH }, { 0.0f, collarH + 0.45f * sb }, { -0.0025f, collarH + 0.80f * sb },
                                              { -0.0060f, collarH + sb }, { -0.012f, collarH + 0.95f * sb },
                                              { -0.45f * w, collarH + sb + 0.10f * r }, { -0.80f * w, collarH + sb + 0.20f * r }, { -w, collarH + sb + 0.24f * r } };
            Part skirt { lathe (k.skirtR * r, skirtProf, seg, false, knurl), k.skirtMetal ? Role::metal : Role::body, true, k.skirtColour };
            skirt.polish = 0.5f;
            skirt.brushedRings = k.skirtMetal;
            m.parts.push_back (std::move (skirt));
        }

        Part body { lathe (r, prof, seg, true, grip), k.bodyMetal ? Role::metal : Role::body, true, k.bodyColour };
        body.polish = k.polish;
        body.gloss = k.gloss;
        body.brushedRings = k.brushed;
        if (! carved && k.ribs > 0 && ! k.bodyMetal)
        {
            body.ridges = (float) k.ribs;
            body.ridgesBelowY = bandTo;
        }
        m.parts.push_back (std::move (body));

        // Cap
        float capTop = topY;
        if (k.capR > 0.0f)
        {
            const float cr = std::min (k.capR * r, topR - 0.003f);
            const Vec3 colour = k.capMaterial == 2 ? accent : k.capColour;
            std::vector<ProfilePoint> cp = k.capDome ? std::vector<ProfilePoint> { { 0.0f, topY - 0.003f }, { 0.0f, topY + 0.002f }, { -cr * 0.3f, topY + 0.006f }, { -cr * 0.7f, topY + 0.008f } }
                                                     : std::vector<ProfilePoint> { { 0.0f, topY - 0.003f }, { 0.0f, topY + 0.0025f }, { -0.0025f, topY + 0.0045f } };
            // 0: plastic like the body, 1: polished metal, 2: anodised in the unit's colour, 3: coloured plastic (console caps)
            const bool metal = k.capMaterial == 1 || k.capMaterial == 2;
            Part cap { lathe (cr, cp, seg, true), metal ? Role::metal : Role::body, true, k.capMaterial == 0 ? k.bodyColour : colour };
            cap.polish = k.capMaterial == 1 ? 0.8f : 0.32f;
            cap.brushedRings = metal;
            m.parts.push_back (std::move (cap));
            capTop = topY + (k.capDome ? 0.008f : 0.0045f);
        }

        // Pointer
        const float py = capTop + 0.0008f;
        switch (k.pointer)
        {
            case KnobRecipe::line:
                m.parts.push_back ({ geo::box ({ -0.0055f, py - 0.0012f, -(topR - 0.006f) }, { 0.0055f, py + 0.0012f, -topR * 0.18f }), Role::pointer, true, k.pointerColour });
                break;
            case KnobRecipe::notch:
                m.parts.push_back ({ geo::box ({ -0.0045f, py - 0.0012f, -(topR - 0.004f) }, { 0.0045f, py + 0.0006f, -topR * 0.35f }), Role::body, true, k.pointerColour });
                break;
            case KnobRecipe::dot:
            {
                MeshData d;
                d.append (lathe (std::max (0.006f, topR * 0.11f), { { 0.0f, py - 0.0015f }, { 0.0f, py + 0.0012f } }, std::max (16, seg / 4), true),
                          Mat4::translation ({ 0.0f, 0.0f, -topR * 0.68f }));
                m.parts.push_back ({ std::move (d), k.pointerColour.x < 0.1f ? Role::body : Role::pointer, true, k.pointerColour });
                break;
            }
            case KnobRecipe::skirtLine:
                m.parts.push_back ({ geo::box ({ -0.0065f, skirtH - 0.0015f, -(k.skirtR * r - 0.003f) }, { 0.0065f, skirtH + 0.0016f, -(r - 0.002f) }), Role::pointer, true, k.pointerColour });
                break;
            case KnobRecipe::bar:
            {
                const float len = r * 1.55f, w = r * 0.40f;
                m.parts.push_back ({ geo::pointerPlate (len, r * 0.55f, w, y0, y0 + h * 0.85f), k.bodyMetal ? Role::metal : Role::body, true, k.bodyColour });
                m.parts.push_back ({ geo::box ({ -0.005f, y0 + h * 0.85f - 0.0006f, -(len - 0.008f) }, { 0.005f, y0 + h * 0.85f + 0.0025f, -r * 0.2f }), Role::pointer, true, k.pointerColour });
                m.footprintRadius = std::max (m.footprintRadius, len);
                m.shadowRadius = r;
                m.beakLength = len;
                m.beakHalfWidth = w;
                break;
            }
            case KnobRecipe::sideLine:
            {
                // Painted on: across the top from near the centre to the edge, then down the flank to the
                // base (or the skirt) - how most studio knobs are marked
                m.parts.push_back ({ geo::box ({ -0.0055f, py - 0.0012f, -(topR - 0.002f) }, { 0.0055f, py + 0.0012f, -topR * 0.22f }), Role::pointer, true, k.pointerColour });
                const float lift = 0.0018f + (carved ? 0.0f : 0.0f);
                const float bottomY = y0 + 0.03f * h, bottomR = r + lift + (k.ribs > 0 ? 0.0f : 0.0f);
                MeshData side;
                side.append (geo::quad ({ -0.0055f, bottomY, -bottomR }, { 0.0055f, bottomY, -bottomR },
                                        { 0.0055f, topY - 0.003f, -(topR + lift) }, { -0.0055f, topY - 0.003f, -(topR + lift) }));
                m.parts.push_back ({ std::move (side), Role::pointer, true, k.pointerColour });
                break;
            }
            case KnobRecipe::wing:
            {
                m.parts.push_back ({ geo::box ({ -r * 0.16f, topY - 0.02f, -r * 1.02f }, { r * 0.16f, topY + 0.010f, r * 1.02f }), Role::body, true, k.bodyColour });
                m.parts.push_back ({ geo::box ({ -0.005f, topY + 0.0095f, -r }, { 0.005f, topY + 0.0125f, -r * 0.2f }), Role::pointer, true, k.pointerColour });
                break;
            }
        }

        m.footprintRadius = std::max ({ m.footprintRadius, r * 1.02f, k.skirtR * r });
        if (m.shadowRadius <= 0.0f)
            m.shadowRadius = std::max (r, k.skirtR * r);
        m.height = capTop;
        return m;
    }

    Model knob (KnobStyle style, float r, Vec3 accent, int detail)
    {
        using geo::lathe;
        using geo::Relief;

        Model m;
        m.footprintRadius = r * 1.12f;
        detail = std::clamp (detail, 0, numDetailLevels - 1);
        const int seg = segmentsFor (detail);
        const bool carved = detail >= 2;     // close up: grip detail is real geometry, not shading

        switch (style)
        {
            case KnobStyle::proXl:
            {
                const float top = r * 1.18f, flange = r * 1.11f;
                // Flange, a shallow undercut where it meets the panel, the ridged flank, then a
                // chamfer and a slightly dished top.
                Relief grip;
                if (carved)
                    grip = { 28, 0.035f, top * 0.22f, top - 0.024f, 0.006f, 0.35f };
                Part body { lathe (r, { { flange - r, 0.0f }, { flange - r, top * 0.10f },
                                        { flange - r - 0.004f, top * 0.13f }, { 0.0f, top * 0.19f },
                                        { -0.003f, top - 0.020f }, { -0.010f, top - 0.005f },
                                        { -0.022f, top }, { -0.030f, top - 0.004f } }, seg, true, grip),
                            Role::body, true, { 0.030f, 0.030f, 0.033f }, carved ? 0.0f : 28.0f, top - 0.024f };
                m.parts.push_back (std::move (body));
                if (carved)
                {
                    // Brushed aluminium insert set into the dished top
                    Part insert { lathe (r * 0.56f, { { 0.0f, top - 0.0050f }, { 0.0f, top - 0.0035f }, { -0.002f, top - 0.0025f } }, seg, true),
                                  Role::metal, true, { 0.70f, 0.70f, 0.72f } };
                    insert.polish = 0.40f;
                    insert.brushedRings = true;
                    m.parts.push_back (std::move (insert));
                }
                m.parts.push_back ({ pointerLine (r, top - 0.0015f, 0.0065f, 0.030f), Role::pointer, true, { 0.92f, 0.93f, 0.95f } });
                m.height = top;
                break;
            }

            case KnobStyle::fluted:
            {
                const float skirtR = r * 1.32f, top = r * 1.20f;
                m.footprintRadius = skirtR;
                m.parts.push_back ({ lathe (skirtR, { { 0.0f, 0.0f }, { 0.0f, 0.010f }, { -0.003f, 0.014f },
                                                     { -0.010f, 0.022f }, { -skirtR + r + 0.004f, 0.026f },
                                                     { -skirtR + r, 0.030f } }, seg, true),
                                     Role::body, true, { 0.025f, 0.025f, 0.028f } });
                Relief flutes;
                if (carved)
                    flutes = { 18, 0.075f, 0.040f, top - 0.030f, 0.010f, 0.12f };   // rounded Davies-type flutes
                m.parts.push_back ({ lathe (r, { { 0.0f, 0.026f }, { -0.004f, 0.040f }, { -0.006f, top - 0.026f },
                                                 { -0.013f, top - 0.008f }, { -0.026f, top }, { -0.034f, top - 0.005f } }, seg, true, flutes),
                                     Role::body, true, { 0.030f, 0.030f, 0.033f }, carved ? 0.0f : 16.0f, top - 0.030f });
                m.parts.push_back ({ pointerLine (r, top, 0.006f, 0.0f), Role::pointer, true, { 0.95f, 0.95f, 0.97f } });
                // pointer continues down the skirt
                m.parts.push_back ({ geo::box ({ -0.005f, 0.021f, -(skirtR - 0.006f) }, { 0.005f, 0.027f, -(r - 0.004f) }), Role::pointer, true, { 0.95f, 0.95f, 0.97f } });
                m.height = top;
                break;
            }

            case KnobStyle::chickenHead:
            case KnobStyle::chickenHeadKnob:
            {
                // Black bakelite: a round back end that tapers to a pointed beak, white inlay along it.
                // As a selector the beak reaches out over the positions; as a knob it stops at the ticks.
                const float top = r * 0.95f;
                const float beak = style == KnobStyle::chickenHeadKnob ? 1.26f : 1.55f;
                m.parts.push_back ({ lathe (r * 0.95f, { { 0.0f, 0.0f }, { 0.0f, 0.03f }, { -0.012f, 0.045f } }, seg, true),
                                     Role::body, true, { 0.035f, 0.030f, 0.028f } });
                m.parts.push_back ({ geo::pointerPlate (r * beak, r * 0.55f, r * 0.42f, 0.04f, top), Role::body, true, { 0.035f, 0.030f, 0.028f } });
                m.parts.push_back ({ geo::box ({ -0.006f, top - 0.0005f, -(r * (beak - 0.10f)) }, { 0.006f, top + 0.003f, -r * 0.1f }), Role::pointer, true, { 0.96f, 0.94f, 0.88f } });
                m.footprintRadius = r * (beak + 0.05f);
                m.shadowRadius = r * 0.98f;        // the body; the beak casts its own, turning shadow
                m.beakLength = r * (beak - 0.05f);
                m.beakHalfWidth = r * 0.42f;
                m.height = top;
                break;
            }

            case KnobStyle::aluminium:
            {
                const float top = r * 1.05f;
                Relief knurl;
                if (carved)
                    knurl = { 72, 0.022f, 0.012f, top - 0.020f, 0.004f, 0.9f };   // fine straight V-knurl
                Part cap { lathe (r, { { 0.0f, 0.0f }, { 0.0f, 0.006f }, { -0.002f, 0.010f },
                                       { -0.002f, top - 0.016f }, { -0.008f, top - 0.004f },
                                       { -0.018f, top }, { -0.026f, top - 0.003f } }, seg, true, knurl),
                           Role::metal, true, { 0.80f, 0.80f, 0.82f } };
                cap.polish = 0.45f;
                cap.brushedRings = true;
                m.parts.push_back (std::move (cap));
                // engraved indicator dot near the edge
                MeshData dot;
                dot.append (lathe (r * 0.12f, { { 0.0f, top - 0.0005f }, { 0.0f, top + 0.0025f } }, std::max (16, seg / 4), true),
                            Mat4::translation ({ 0.0f, 0.0f, -r * 0.68f }));
                m.parts.push_back ({ std::move (dot), Role::body, true, { 0.02f, 0.02f, 0.02f } });
                m.height = top;
                break;
            }

            case KnobStyle::softTouch:
            {
                const float top = r * 1.15f;
                m.parts.push_back ({ lathe (r, { { 0.005f, 0.0f }, { 0.004f, 0.010f }, { 0.0f, top * 0.78f },
                                                 { -0.008f, top * 0.92f }, { -0.026f, top } }, seg, true),
                                     Role::body, true, { 0.23f, 0.23f, 0.25f }, 36.0f, top * 0.76f });
                if (carved)   // a thin polished trim ring round the cap
                {
                    Part trim { lathe (r * 0.66f, { { 0.0f, top - 0.002f }, { 0.0f, top + 0.003f }, { -0.003f, top + 0.005f } }, seg, false),
                                Role::metal, true, { 0.86f, 0.86f, 0.88f } };
                    trim.polish = 0.85f;
                    m.parts.push_back (std::move (trim));
                }
                m.parts.push_back ({ lathe (r * 0.62f, { { 0.0f, top - 0.004f }, { -0.006f, top + 0.006f } }, seg, true),
                                     Role::accent, true, accent });
                m.parts.push_back ({ geo::box ({ -0.005f, top + 0.006f, -(r * 0.60f) }, { 0.005f, top + 0.0085f, -r * 0.12f }), Role::pointer, true, { 0.97f, 0.97f, 0.99f } });
                m.height = top + 0.006f;
                break;
            }

            case KnobStyle::jewelCap:
            {
                const float top = r * 1.2f;
                m.parts.push_back ({ lathe (r, { { 0.008f, 0.0f }, { 0.0f, 0.03f }, { 0.0f, top - 0.03f } }, seg, false),
                                     Role::body, true, { 0.03f, 0.03f, 0.034f }, 40.0f, top - 0.03f });
                Part cap { lathe (r, { { 0.0f, top - 0.03f }, { 0.0f, top - 0.012f }, { -0.014f, top } }, seg, true),
                           Role::metal, true, { 0.86f, 0.85f, 0.88f } };
                cap.polish = 0.9f;
                m.parts.push_back (std::move (cap));
                m.parts.push_back ({ lathe (r * 0.30f, { { 0.0f, top }, { -r * 0.12f, top + 0.018f } }, seg / 2, true),
                                     Role::accent, true, accent });
                m.parts.push_back ({ pointerLine (r, top + 0.001f, 0.005f, r * 0.42f), Role::pointer, true, { 0.97f, 0.97f, 0.99f } });
                m.height = top + 0.018f;
                break;
            }

            case KnobStyle::skirted:
            {
                // Outboard-gear knob: machined skirt, black ribbed cone, white line down the flank, and
                // (close up) a set screw in the skirt and an anodised cap insert in the unit's colour.
                const float skirtR = r * 1.26f, top = r * 1.05f;
                m.footprintRadius = skirtR;

                Relief skirtKnurl;
                if (carved)
                    skirtKnurl = { 96, 0.014f, 0.003f, 0.017f, 0.002f, 0.9f };
                Part skirt { lathe (skirtR, { { 0.0f, 0.0f }, { 0.0f, 0.014f }, { -0.002f, 0.018f },
                                              { -0.002f, 0.024f }, { -0.008f, 0.030f } }, seg, true, skirtKnurl),
                             Role::metal, true, { 0.74f, 0.74f, 0.77f } };
                skirt.polish = 0.42f;
                skirt.brushedRings = true;
                m.parts.push_back (std::move (skirt));

                Relief ribs;
                if (carved)
                    ribs = { 44, 0.030f, 0.036f, top - 0.022f, 0.006f, 0.30f };
                m.parts.push_back ({ lathe (r, { { 0.0f, 0.028f }, { -0.003f, 0.040f }, { -0.006f, 0.052f },
                                                 { -r * 0.30f, top - 0.012f }, { -r * 0.38f, top - 0.002f },
                                                 { -r * 0.48f, top - 0.008f } }, seg, true, ribs),
                                     Role::body, true, { 0.035f, 0.035f, 0.038f }, carved ? 0.0f : 44.0f, top - 0.02f });

                // Pointer: a line down the sloping flank, reaching the skirt
                m.parts.push_back ({ geo::box ({ -0.0055f, 0.024f, -(skirtR - 0.004f) }, { 0.0055f, 0.030f, -(r - 0.012f) }),
                                     Role::pointer, true, { 0.96f, 0.96f, 0.98f } });

                if (carved)
                {
                    // Anodised cap insert, in the unit's colour (muted), with the pointer line across it
                    Part cap { lathe (r * 0.40f, { { 0.0f, top - 0.0085f }, { 0.0f, top - 0.004f }, { -0.003f, top - 0.0015f } }, seg, true),
                               Role::metal, true, accent };
                    cap.polish = 0.30f;
                    cap.brushedRings = true;
                    m.parts.push_back (std::move (cap));
                    m.parts.push_back ({ geo::box ({ -0.0045f, top - 0.0020f, -(r * 0.40f - 0.004f) }, { 0.0045f, top - 0.0005f, -r * 0.08f }),
                                         Role::pointer, true, { 0.96f, 0.96f, 0.98f } });

                    // Set screw in the skirt, opposite the pointer
                    MeshData screw;
                    screw.append (lathe (0.0055f, { { 0.0f, -0.0015f }, { 0.0f, 0.0015f } }, 16, true),
                                  Mat4::translation ({ 0.0f, 0.010f, skirtR - 0.0015f }) * Mat4::rotationX (0.5f * geo::kPi));
                    m.parts.push_back ({ std::move (screw), Role::body, true, { 0.012f, 0.012f, 0.014f } });
                }
                else
                {
                    m.parts.push_back ({ pointerLine (r * 0.78f, top - 0.004f, 0.0055f, r * 0.10f),
                                         Role::pointer, true, { 0.96f, 0.96f, 0.98f } });
                }
                m.shadowRadius = skirtR;
                m.height = top;
                break;
            }

            default:
                if (auto* recipe = recipeFor (style))
                    return recipeKnob (*recipe, r, accent, detail);
                break;
        }

        return m;
    }

    //==============================================================================
    Model vuMeter (float halfW, float halfH, float depth, Vec3 bezelColour, bool flush)
    {
        Model m;
        m.footprintRadius = std::max (halfW, halfH);
        m.height = 0.0f;

        const geo::Rect window { 0.0f, 0.0f, halfW, halfH };
        const geo::Rect inner { 0.0f, 0.0f, halfW - 0.012f, halfH - 0.010f };

        // The case behind the face, and the walls of the recess
        m.parts.push_back ({ geo::wellWalls (window, 0.0f, depth), Role::body, false, { 0.05f, 0.05f, 0.055f } });

        // Printed face, recessed, uv 0..1 across the window
        m.parts.push_back ({ geo::horizontalQuad (inner, -depth), Role::screen, false, { 0.92f, 0.89f, 0.80f } });

        /*  Needle: hinged below the window so only its tip sweeps the dial. Built from the
            hinge outward along -z, tapering, then moved down to the hinge. The renderer turns
            it about that hinge (Model::pivotOffset). */
        const float pivotDrop = halfH * vuPivotDrop;
        const float reach = halfH * vuNeedleTip;
        MeshData needle;
        needle.append (geo::box ({ -0.0075f, -depth + 0.004f, -reach * 0.62f }, { 0.0075f, -depth + 0.009f, 0.012f }));
        needle.append (geo::box ({ -0.0040f, -depth + 0.004f, -reach }, { 0.0040f, -depth + 0.009f, -reach * 0.62f }));
        auto moved = MeshData();
        moved.append (needle, gfx::Mat4::translation ({ 0.0f, 0.0f, pivotDrop }));
        m.parts.push_back ({ std::move (moved), Role::pointer, true, { 0.10f, 0.10f, 0.12f } });
        m.pivotOffset = pivotDrop;

        // Pivot hub, sitting on the face at the bottom of the dial
        MeshData hub;
        hub.append (sweptRoundedRect (0, 0, halfH * 0.20f, 10,
                                      { { 0.0f, -depth + 0.004f }, { 0.0f, -depth + 0.012f }, { -halfH * 0.07f, -depth + 0.018f } }, true),
                    gfx::Mat4::translation ({ 0.0f, 0.0f, pivotDrop }));
        m.parts.push_back ({ std::move (hub), Role::body, false, { 0.13f, 0.13f, 0.15f } });

        if (flush)
        {
            /*  Flush: no bezel. The glass sits in the panel's own cut-out, a hair below its surface, held
                by a thin black gasket - the way modern studio gear seats its meters. */
            MeshData gasket;
            gasket.append (geo::wellWalls ({ 0.0f, 0.0f, halfW + 0.004f, halfH + 0.004f }, 0.0f, 0.006f));
            gasket.append (geo::plateWithHoles ({ 0.0f, 0.0f, halfW + 0.004f, halfH + 0.004f }, -0.006f, { window }));
            m.parts.push_back ({ std::move (gasket), Role::body, false, { 0.012f, 0.012f, 0.014f } });
            m.parts.push_back ({ geo::horizontalQuad ({ 0.0f, 0.0f, halfW + 0.003f, halfH + 0.003f }, -0.003f),
                                 Role::glass, false, { 0.60f, 0.64f, 0.70f } });
            return m;
        }

        // Bezel: a metal frame standing slightly proud of the panel
        constexpr float rc = 0.016f;
        MeshData bezel;
        bezel.append (sweptRoundedRect (halfW + 0.026f - rc, halfH + 0.024f - rc, rc, 4,
                                        { { 0.030f, 0.0f }, { 0.030f, 0.011f }, { 0.018f, 0.020f },
                                          { 0.004f, 0.020f }, { 0.0f, 0.008f }, { 0.0f, 0.0f } }, false));
        Part frame { std::move (bezel), Role::metal, false, bezelColour };
        frame.polish = 0.55f;
        m.parts.push_back (std::move (frame));

        // Four screws holding the bezel to the panel
        MeshData screws;
        const auto head = sweptRoundedRect (0, 0, 0.019f, 8, { { 0.0f, 0.020f }, { 0.0f, 0.026f }, { -0.008f, 0.030f } }, true);
        for (float sx : { -1.0f, 1.0f })
            for (float sz : { -1.0f, 1.0f })
                screws.append (head, gfx::Mat4::translation ({ sx * (halfW + 0.010f), 0.0f, sz * (halfH + 0.008f) }));
        Part screwPart { std::move (screws), Role::metal, false, { 0.70f, 0.70f, 0.73f } };
        screwPart.polish = 0.45f;
        m.parts.push_back (std::move (screwPart));

        // Cover glass, just inside the bezel
        m.parts.push_back ({ geo::horizontalQuad ({ 0.0f, 0.0f, halfW + 0.004f, halfH + 0.004f }, 0.012f),
                             Role::glass, false, { 0.60f, 0.64f, 0.70f } });
        return m;
    }

    //==============================================================================
    Model pushButton (float halfW, float halfD, int detail)
    {
        Model m;
        constexpr float rc = 0.016f, r = 0.010f;
        const int corner = segmentsFor (detail) / 10 + 3;   // 6 .. 28 per corner
        // Collar: a bezel standing off the panel with a chamfer into the well the cap sits in
        m.parts.push_back ({ sweptRoundedRect (halfW + 0.016f - rc, halfD + 0.016f - rc, rc, corner,
                                               { { 0.0f, 0.0f }, { 0.0f, 0.009f }, { -0.004f, 0.014f },
                                                 { -0.010f, 0.014f }, { -0.012f, 0.008f }, { -0.012f, 0.002f } }, false),
                             Role::body, false, { 0.018f, 0.018f, 0.020f } });

        // Cap: square-ish, chamfered all round, with a dished top that catches the light
        m.parts.push_back ({ sweptRoundedRect (halfW - r, halfD - r, r, corner,
                                               { { 0.0f, 0.004f }, { 0.0f, 0.040f }, { -0.005f, 0.050f },
                                                 { -0.014f, 0.054f }, { -0.022f, 0.051f } }, true),
                             Role::accent, true, { 0.40f, 0.41f, 0.43f } });   // "rotates" = moves with the press
        m.footprintRadius = std::max (halfW, halfD) + 0.014f;
        m.height = 0.054f;
        return m;
    }

    //==============================================================================
    Model rockerSwitch (int detail, Vec3 paddleColour, float widthScale)
    {
        // A panel rocker: chamfered bezel with a dark well, and a satin paddle that rocks about its middle,
        // marked I (on) at the -z end and O (off) at the +z end. The paddle and its marks are the parts
        // that move: rotate them about x at rockerPivotY.
        Model m;
        const int corner = segmentsFor (detail) / 16 + 2;   // 4 .. 18 per corner
        const float bw = rockerHalfW * widthScale, bd = rockerHalfD;
        constexpr float rc = 0.015f;

        m.parts.push_back ({ sweptRoundedRect (bw - rc, bd - rc, rc, corner,
                                               { { 0.0f, 0.0f }, { 0.0f, 0.007f }, { -0.003f, 0.011f }, { -0.008f, 0.012f },
                                                 { -0.0105f, 0.010f }, { -0.0105f, -0.014f } }, false),
                             Role::body, false, { 0.020f, 0.020f, 0.022f } });
        m.parts.push_back ({ geo::horizontalQuad ({ 0.0f, 0.0f, bw - 0.010f, bd - 0.010f }, -0.013f), Role::body, false, { 0.004f, 0.004f, 0.005f } });

        // Paddle, built about its pivot (y = 0 here = rockerPivotY on the panel)
        const float pw = bw - 0.0125f, pd = bd - 0.0125f;
        constexpr float pr = 0.007f, topY = 0.020f;
        Part paddle { sweptRoundedRect (pw - pr, pd - pr, pr, corner,
                                        { { 0.0f, -0.020f }, { 0.0f, topY - 0.004f }, { -0.0015f, topY - 0.001f }, { -0.004f, topY } }, true),
                      Role::body, true, paddleColour };
        m.parts.push_back (std::move (paddle));

        // I and O, raised a hair above the paddle so they print cleanly at any distance
        const float markY = topY + 0.0006f;
        m.parts.push_back ({ geo::box ({ -0.0042f, markY - 0.0004f, -pd + 0.015f }, { 0.0042f, markY + 0.0004f, -pd + 0.046f }),
                             Role::pointer, true, { 0.93f, 0.93f, 0.95f } });
        MeshData o;
        o.append (geo::flatAnnulus (0.0082f, 0.0132f, std::max (16, segmentsFor (detail) / 4)), Mat4::translation ({ 0.0f, markY, pd - 0.031f }));
        m.parts.push_back ({ std::move (o), Role::pointer, true, { 0.93f, 0.93f, 0.95f } });

        m.footprintRadius = bd;
        m.height = rockerPivotY + topY;
        m.shadowRadius = bw;
        m.leverPivotY = rockerPivotY;
        m.leverAngle = rockerAngle;
        m.halfW = bw;
        m.halfD = bd;
        return m;
    }

    const char* switchStyleName (SwitchStyle s) noexcept
    {
        switch (s)
        {
            case SwitchStyle::rocker:       return "Rocker, I / O";
            case SwitchStyle::rockerRed:    return "Rocker, red paddle";
            case SwitchStyle::rockerWide:   return "Rocker, wide";
            case SwitchStyle::batToggle:    return "Bat toggle";
            case SwitchStyle::paddleToggle: return "Paddle toggle";
            default:                        return "Switch";
        }
    }

    Model toggleSwitch (SwitchStyle style, int detail)
    {
        switch (style)
        {
            case SwitchStyle::rockerRed:  return rockerSwitch (detail, { 0.46f, 0.07f, 0.06f }, 1.0f);
            case SwitchStyle::rockerWide: return rockerSwitch (detail, { 0.045f, 0.045f, 0.050f }, 1.45f);
            case SwitchStyle::batToggle:
            case SwitchStyle::paddleToggle:
            {
                // Nut and bushing fixed; the lever (a bat, or a flat paddle) swings about the bushing
                Model m = batToggleBase();
                for (auto& part : m.parts)
                    part.rotates = false;
                if (style == SwitchStyle::batToggle)
                {
                    for (auto& part : batToggleLever().parts)
                        m.parts.push_back (part);
                }
                else
                {
                    Part paddle { geo::sweptRoundedRect (0.010f, 0.003f, 0.006f, std::max (4, segmentsFor (detail) / 16), { { 0.0f, -0.010f }, { 0.0f, 0.140f }, { -0.004f, 0.150f } }, true),
                                  Role::metal, true, { 0.86f, 0.86f, 0.88f } };
                    paddle.polish = 0.7f;
                    m.parts.push_back (std::move (paddle));
                }
                m.leverPivotY = 0.05f;
                m.leverAngle = 28.0f * geo::kPi / 180.0f;
                m.halfW = 0.042f;
                m.halfD = 0.042f + 0.186f * std::sin (m.leverAngle);
                m.footprintRadius = m.halfD;
                m.shadowRadius = 0.042f;
                return m;
            }
            default: return rockerSwitch (detail);
        }
    }

    const char* buttonStyleName (ButtonStyle s) noexcept
    {
        switch (s)
        {
            case ButtonStyle::square:      return "Square latching";
            case ButtonStyle::round:       return "Round";
            case ButtonStyle::wide:        return "Wide";
            case ButtonStyle::chromeBezel: return "Chrome bezel";
            case ButtonStyle::softDome:    return "Soft dome";
            default:                       return "Button";
        }
    }

    Model pushButton (ButtonStyle style, float halfW, float halfD, int detail)
    {
        using geo::lathe;
        const int seg = segmentsFor (detail);
        switch (style)
        {
            case ButtonStyle::round:
            case ButtonStyle::softDome:
            {
                Model m;
                const float r = std::max (halfW, halfD);
                m.parts.push_back ({ lathe (r + 0.016f, { { 0.0f, 0.0f }, { 0.0f, 0.009f }, { -0.004f, 0.014f }, { -0.010f, 0.014f }, { -0.012f, 0.008f }, { -0.012f, 0.002f } }, seg, false),
                                     Role::body, false, { 0.018f, 0.018f, 0.020f } });
                std::vector<ProfilePoint> cap = style == ButtonStyle::round
                    ? std::vector<ProfilePoint> { { 0.0f, 0.004f }, { 0.0f, 0.040f }, { -0.005f, 0.050f }, { -0.012f, 0.054f }, { -0.02f, 0.052f } }
                    : std::vector<ProfilePoint> { { 0.0f, 0.004f }, { 0.0f, 0.030f }, { -0.008f, 0.046f }, { -0.020f, 0.056f }, { -0.035f, 0.060f } };
                m.parts.push_back ({ lathe (r - 0.004f, cap, seg, true), Role::accent, true, { 0.40f, 0.41f, 0.43f } });
                m.footprintRadius = r + 0.016f;
                m.height = 0.06f;
                m.halfW = m.halfD = r + 0.016f;
                return m;
            }
            case ButtonStyle::wide:        { auto m = pushButton (halfW * 1.45f, halfD * 0.85f, detail); m.halfW = halfW * 1.45f + 0.016f; m.halfD = halfD * 0.85f + 0.016f; return m; }
            case ButtonStyle::chromeBezel:
            {
                auto m = pushButton (halfW, halfD, detail);
                m.parts[0].role = Role::metal;
                m.parts[0].colour = { 0.82f, 0.82f, 0.85f };
                m.parts[0].polish = 0.75f;
                m.halfW = halfW + 0.016f;
                m.halfD = halfD + 0.016f;
                return m;
            }
            default: { auto m = pushButton (halfW, halfD, detail); m.halfW = halfW + 0.016f; m.halfD = halfD + 0.016f; return m; }
        }
    }

    Model batToggleBase()
    {
        Model m;
        // Dress nut: hex, with a chamfer top and bottom like a real switch nut
        Part nut { geo::sweptPolygon (6, 0.042f, { { 0.0f, 0.0f }, { -0.003f, 0.004f }, { -0.003f, 0.018f },
                                                   { -0.007f, 0.024f }, { -0.013f, 0.026f } }, true),
                   Role::metal, false, { 0.80f, 0.79f, 0.83f } };
        nut.polish = 0.5f;
        m.parts.push_back (std::move (nut));

        // Threaded bushing above it, with a washer at its base
        Part washer { sweptRoundedRect (0, 0, 0.038f, 10, { { 0.0f, 0.026f }, { 0.0f, 0.030f }, { -0.004f, 0.032f } }, true),
                      Role::metal, false, { 0.74f, 0.73f, 0.76f } };
        washer.polish = 0.35f;
        m.parts.push_back (std::move (washer));

        Part bushing { sweptRoundedRect (0, 0, 0.025f, 12, { { 0.0f, 0.030f }, { 0.0f, 0.056f },
                                                             { -0.004f, 0.062f }, { -0.010f, 0.064f } }, true),
                       Role::metal, false, { 0.80f, 0.79f, 0.83f } };
        bushing.polish = 0.6f;
        m.parts.push_back (std::move (bushing));
        m.footprintRadius = 0.042f;
        return m;
    }

    Model batToggleLever()
    {
        Model m;
        MeshData lever;
        // Shaft: tapers as it rises, as a bat lever does
        lever.append (sweptRoundedRect (0, 0, 0.011f, 10, { { 0.002f, -0.010f }, { 0.002f, 0.020f },
                                                            { -0.001f, 0.080f }, { -0.004f, 0.150f } }, false));
        // Ball tip: a proper sphere-ish cap rather than a blob
        lever.append (sweptRoundedRect (0, 0, 0.019f, 12, { { -0.019f, 0.142f }, { -0.012f, 0.148f }, { -0.004f, 0.156f },
                                                            { 0.0f, 0.164f }, { -0.004f, 0.172f }, { -0.012f, 0.180f },
                                                            { -0.019f, 0.186f } }, true));
        Part p { std::move (lever), Role::metal, true, { 0.92f, 0.90f, 0.93f } };
        p.polish = 0.9f;
        m.parts.push_back (std::move (p));
        return m;
    }

    Model jewelLamp (float radius)
    {
        Model m;
        Part bezel { sweptRoundedRect (0, 0, radius, 8, { { 0.0f, 0.0f }, { 0.0f, 0.014f }, { -0.010f, 0.026f }, { -0.022f, 0.026f }, { -0.026f, 0.020f } }, false),
                     Role::metal, false, { 0.85f, 0.84f, 0.88f } };
        bezel.polish = 0.75f;
        m.parts.push_back (std::move (bezel));
        const float j = radius * 0.69f;
        m.parts.push_back ({ geo::sweptPolygon (8, j, { { 0.0f, 0.010f }, { 0.0f, 0.030f }, { -j * 0.44f, 0.058f }, { -j * 0.8f, 0.066f } }, true),
                             Role::pointer, false, { 1.3f, 0.10f, 0.05f } });
        m.footprintRadius = radius;
        return m;
    }

    MeshData ledLens()
    {
        /*  A moulded LED, not a hemisphere: a short cylindrical body with the flange that
            comes out of the mould, then the domed lens on top. Unit radius; scale when drawing. */
        MeshData m;
        m.append (sweptRoundedRect (0, 0, 1.06f, 12, { { 0.0f, 0.0f }, { 0.0f, 0.16f }, { -0.06f, 0.22f } }, false));
        m.append (sweptRoundedRect (0, 0, 1.0f, 12, { { 0.0f, 0.20f }, { 0.0f, 0.40f } }, false));
        m.append (geo::dome (1.0f, 0.62f, 48, 8), gfx::Mat4::translation ({ 0.0f, 0.40f, 0.0f }));
        return m;
    }

    Model rackScrew (float radius)
    {
        Model m;
        Part head { sweptRoundedRect (0, 0, radius, 3, { { 0.0f, 0.0f }, { 0.0f, 0.012f }, { -0.02f, 0.03f } }, true), Role::metal, false, { 0.75f, 0.74f, 0.78f } };
        head.polish = 0.5f;
        m.parts.push_back (std::move (head));
        MeshData recess;
        const float a = radius * 0.44f, t = radius * 0.09f;
        recess.append (geo::box ({ -a, 0.026f, -t }, { a, 0.0315f, t }));
        recess.append (geo::box ({ -t, 0.026f, -a }, { t, 0.0315f, a }));
        m.parts.push_back ({ std::move (recess), Role::body, false, { 0.02f, 0.02f, 0.025f } });
        m.footprintRadius = radius;
        return m;
    }

    MeshData rackChassis (float halfW, float bottomY, float topY, float frontZ, float depth, float seamY)
    {
        constexpr float r = 0.05f;
        const float halfD = 0.5f * depth;
        std::vector<ProfilePoint> profile { { 0.0f, bottomY } };
        if (seamY > bottomY && seamY < topY - 0.05f)
        {
            profile.push_back ({ 0.0f, seamY - 0.012f });
            profile.push_back ({ -0.010f, seamY - 0.004f });
            profile.push_back ({ -0.010f, seamY + 0.004f });
            profile.push_back ({ 0.0f, seamY + 0.012f });
        }
        profile.push_back ({ 0.0f, topY - 0.03f });
        profile.push_back ({ -0.03f, topY });

        MeshData mesh;
        mesh.append (sweptRoundedRect (halfW - r, halfD - r, r, 3, profile, false), Mat4::translation ({ 0.0f, 0.0f, frontZ - halfD }));
        return mesh;
    }

    MeshData faceplateEdge (float halfW, float halfH, float thickness)
    {
        constexpr float r = 0.022f;
        return sweptRoundedRect (halfW - r, halfH - r, r, 2, { { 0.0f, -thickness }, { 0.0f, -0.016f }, { -0.016f, 0.0f } }, false);
    }
}
