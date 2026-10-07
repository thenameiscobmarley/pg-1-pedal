#pragma once

/*  HardwareKit model library: ready-made hardware parts built from the geometry primitives.

    A model is a list of parts. Each part carries a *role* telling the renderer which kind of material
    to use and whether it turns with the control. Sizes are in "panel units"; the default knob radius
    matches a small studio knob on a 19" rack face that is 5 units wide.

        Role::body     main plastic / rubber body     (plastic material, `colour`, optional grip ridges)
        Role::metal    chrome / brushed / aluminium   (chrome material, `polish`, optional brushed rings)
        Role::pointer  indicator line / dot / inlay   (emissive, bright)
        Role::accent   coloured cap or insert         (plastic material in the style's accent colour)
        Role::screen   printed meter face / dial      (the caller's own display material, uv 0..1)
        Role::glass    cover glass over a screen      (drawn last, transparent, reflective)
*/
namespace hwk::models
{
    using gfx::MeshData;
    using gfx::Vec3;

    enum class Role { body, metal, pointer, accent, screen, glass };

    struct Part
    {
        MeshData mesh;
        Role role = Role::body;
        bool rotates = true;          // turns with the control
        Vec3 colour { 0.03f, 0.03f, 0.033f };
        float ridges = 0.0f;          // grip ridges drawn by the plastic material (0 = smooth)
        float ridgesBelowY = 0.0f;    // ridges only below this height
        float polish = 0.6f;          // metal: 0 satin .. 1 mirror
        float gloss = 1.0f;           // plastic: 1 glossy (lacquered) .. 0 matte (soft-touch, bead-blasted)
        bool brushedRings = false;    // metal: concentric machining rings
    };

    struct Model
    {
        std::vector<Part> parts;
        float footprintRadius = 0.12f;   // for picking: covers everything the model can sweep
        float height = 0.13f;

        /** Contact shadow: the round body, plus an optional pointer ("beak") that turns with the
            knob. shadowRadius 0 means "use footprintRadius" - right for knobs that are round. */
        float shadowRadius = 0.0f;
        float beakLength = 0.0f, beakHalfWidth = 0.0f;

        /** Meters: where the needle is hinged, in +z from the model centre (0 = rotate about
            the centre, as a knob does). The renderer must rotate the moving part about it. */
        float pivotOffset = 0.0f;

        /** Switches: the moving parts turn about x at this height, by -leverAngle when on, +leverAngle when off. */
        float leverPivotY = 0.0f, leverAngle = 0.0f;
        float halfW = 0.0f, halfD = 0.0f;   // switches / buttons: the fixed outline on the panel

        float bodyShadowRadius() const noexcept { return shadowRadius > 0.0f ? shadowRadius : footprintRadius; }
    };

    //==============================================================================
    /** Knob styles. All point at -z when their value is centred (angle 0) and turn about +y. */
    enum class KnobStyle
    {
        proXl,        // black ridged cylinder with a white line (modern rack processor)
        fluted,       // Davies-1900-type: fluted body on a wide skirt, white pointer line
        chickenHead,  // bakelite pointer knob with a white inlay, for selectors
        aluminium,    // machined aluminium cap with knurled flank and an engraved dot
        softTouch,    // grey soft-touch rubber with a coloured cap (console style)
        jewelCap,     // black body with a polished metal cap and a coloured jewel centre
        skirted,      // outboard-gear type: black cone on a machined metal skirt, white pointer
        chickenHeadKnob, // the same black bakelite chicken head, its beak ending at a knob's printed ticks

        // Recipe styles (see knobRecipe): console, vintage, machined, instrument, hi-fi, guitar
        consoleAccent, consoleRed, consoleBlue, consoleGreen, consoleYellow, consoleWhite, consoleLow,
        marconi, bakelitePointer, creamRadio, bakeliteFluted, chromeSkirtCone, wingPointer, broadcastBrass, daviesSmall,
        machinedSilver, machinedBlack, machinedGunmetal, brassKnurl, crosshatchSilver, steppedAluminium, chromeDome,
        colletBlack, colletGrey, colletCream, rogan, roganSkirt, rubberTall, rubberLow, rubberRibbed,
        pointerBarBlack, pointerBarSilver, milSpecPointer,
        hifiDisc, hifiBlackDisc, hifiDimple, gunmetalCap,
        guitarTopHat, guitarSpeed, guitarDome, amberInstrument, oxbloodInstrument,
        // Classic studio gear (from photographs): the program EQ's fluted bakelite top hat, the FET limiter's
        // knurled aluminium, the optical leveler's fluted skirt, a small ribbed black, the modern channel's
        // smooth matte black and red anodised trim, the vintage channel's maroon, dark grey and small grey,
        // the 500-series coloured caps, the passive mastering EQ's ribbed pointer
        pultecTopHat, fetSilver, la2aFluted, smallRibbed, porticoBlack, porticoRed,
        neveMaroon, neveGrey, neveSmallGrey, apiBlue, apiRed, apiWhite, manleyRibbed,
        // Made in the ENH Master Rack Unit Designer (its knob maker): TAKEBACK's grey ribbed metal
        greyRibbedMetal,
        count
    };

    inline constexpr int numKnobStyles = (int) KnobStyle::count;

    /** Every style, in order (for galleries and tests). */
    inline std::vector<KnobStyle> allKnobStyles()
    {
        std::vector<KnobStyle> v;
        for (int i = 0; i < numKnobStyles; ++i)
            v.push_back ((KnobStyle) i);
        return v;
    }

    const char* knobStyleName (KnobStyle) noexcept;

    /** Levels of detail every model can be built at: 32, 64, 128 and 256 segments around. From level 2
        up, grip ridges, flutes and knurling are carved into the geometry (below it the plastic material
        paints them), and the finer machined parts appear: inserts, trim rings, set screws.
        Pick the level from the part's size on screen - see detailForPixels(). */
    inline constexpr int numDetailLevels = 4;
    int segmentsFor (int detail) noexcept;

    /** The level of detail for a round part `radiusPixels` across on screen (radius, in pixels). */
    inline int detailForPixels (float radiusPixels) noexcept
    {
        return radiusPixels < 12.0f ? 0 : radiusPixels < 28.0f ? 1 : radiusPixels < 70.0f ? 2 : 3;
    }

    /** radius = body radius; accent colours the cap / insert where the style has one. */
    Model knob (KnobStyle, float radius = 0.114f, Vec3 accent = { 0.42f, 0.28f, 0.86f }, int detail = 1);

    //==============================================================================
    /** Latching square push button: collar (fixed) + cap (moves, does not rotate). */
    Model pushButton (float halfW = 0.060f, float halfD = 0.042f, int detail = 1);

    /** Button styles. */
    enum class ButtonStyle { square, round, wide, chromeBezel, softDome, count };
    inline constexpr int numButtonStyles = (int) ButtonStyle::count;
    const char* buttonStyleName (ButtonStyle) noexcept;

    /** A latching push button in any style: collar (fixed) + cap (moves, does not rotate). */
    Model pushButton (ButtonStyle, float halfW, float halfD, int detail);

    /** Two-position switch styles. Every one is a Model whose moving parts turn about x at
        Model::leverPivotY by -/+ Model::leverAngle (on / off), so a renderer draws them all alike. */
    enum class SwitchStyle { rocker, rockerRed, rockerWide, batToggle, paddleToggle, count };
    inline constexpr int numSwitchStyles = (int) SwitchStyle::count;
    const char* switchStyleName (SwitchStyle) noexcept;
    Model toggleSwitch (SwitchStyle, int detail);

    /** I / O rocker switch: bezel and well (fixed), paddle with its I and O marks (rotates about x at
        rockerPivotY: -rockerAngle = the I end pressed in = on, +rockerAngle = off). */
    inline constexpr float rockerHalfW = 0.060f, rockerHalfD = 0.098f, rockerPivotY = 0.005f;
    inline constexpr float rockerAngle = 11.0f * 3.14159265f / 180.0f;
    Model rockerSwitch (int detail = 1, Vec3 paddleColour = { 0.045f, 0.045f, 0.050f }, float widthScale = 1.0f);

    /** Bat toggle: hex nut + bushing (fixed) and a lever part that pivots about x at `pivotY`. */
    Model batToggleBase();
    Model batToggleLever();
    inline constexpr float batTogglePivotY = 0.05f;

    /** Faceted jewel lamp in a chrome bezel. */
    Model jewelLamp (float radius = 0.072f);

    /** Round LED lens (unit radius; scale when drawing). */
    MeshData ledLens();

    /** Rack screw with cross recess (head + recess as separate parts). */
    Model rackScrew (float radius = 0.05f);

    /** Rounded rack chassis body between two heights, front at chassisFrontZ, depth toward -z. */
    MeshData rackChassis (float halfW, float bottomY, float topY, float frontZ, float depth, float seamY = -1.0f);

    /** Faceplate outer bevel (panel-local, face at y = 0, thickness toward -y). */
    MeshData faceplateEdge (float halfW, float halfH, float thickness);

    //==============================================================================
    /** A moving-coil VU meter, the kind bolted into outboard gear: a metal bezel around a
        recessed printed face, a needle on a pivot below the face, a hub, and cover glass.

        Built panel-local around the centre of its window (x across, z down, y out of the panel).
        The needle part rotates about +y, so the same machinery that turns a knob swings it:
        angle 0 points straight up, negative to the left.

        Parts, in order: case, bezel, face (Role::screen, uv 0..1 across the window),
        needle (Role::pointer, rotates), hub, glass (Role::glass).
        The needle pivot sits `pivotDrop` below the bottom of the face, off the visible dial. */
    Model vuMeter (float halfW, float halfH, float depth, Vec3 bezelColour = { 0.10f, 0.11f, 0.13f }, bool flush = false);   // flush: no bezel, glass set into the panel

    /** Sweep of a VU needle: angle for a 0..1 reading (radians about +y). */
    inline constexpr float vuSweep = 0.92f;   // ~53 degrees total
    inline float vuAngleFor (float normalised) noexcept { return (normalised - 0.5f) * vuSweep; }

    /** The dial geometry, shared by the model and by whoever prints the face:
        the hinge sits `pivotDrop` below the centre of the window, and the needle tip sweeps
        an arc of radius `needleReach` around it. Both are multiples of the window half-height. */
    inline constexpr float vuPivotDrop  = 1.35f;
    inline constexpr float vuNeedleTip  = 2.07f;
    inline constexpr float vuArcRadius  = 1.92f;
}
