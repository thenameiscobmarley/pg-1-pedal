#pragma once

#include <array>

/*  HardwareKit materials.

    One shared vertex shader; each material is a GLSL snippet placed inside a common fragment main()
    (lighting terms already computed: N V L H R ndl wrap ndv ndh facing amb fill lightCol envColor()).
    The snippet writes `col` (and `alpha`). Materials are compiled once each, so the GPU never branches
    per fragment - important on integrated graphics.

    Common uniforms: uModel uViewProj uCamPos uLightDir uTime uViewport uVignette
                     uBaseColor uEmissive uParams uParams2 uGlow uTex uTex2
    A material may declare its own extra uniforms (`declarations`).
*/
namespace hwk::shaders
{
    struct Material
    {
        const char* name = "";
        const char* body = "";              // GLSL statements writing col / alpha
        const char* declarations = "";      // extra uniforms / helper functions
        bool postProcess = true;            // highlight roll-off, vignette (uVignette), dither
    };

    extern const char* const vertex;

    /** Full fragment shader source ("#version 150" included). */
    juce::String fragmentSource (const Material&);

    /** A surface's three wear scratches (see wearMarks in the shader header), worked out once on the CPU
        instead of for every pixel: per scratch its two ends and its opacity (ax, ay, bx, by, opacity).
        Set as the uWear uniform on the panel materials; `seed` is what the material used to pass
        (the unit's seed plus the material's offset: anodised 0, lacquer 3, brushed 7). */
    std::array<float, 15> wearUniforms (float seed, float scale = 1.0f) noexcept;

    /** The room's diffuse light for uSH (set uShOn to 1): the room map (uEnv's format: equirectangular,
        RGBA bytes storing sqrt (colour / range)) projected onto nine spherical harmonics and turned into
        irradiance, 27 floats (nine RGB terms). `meanLuma` > 0 scales it so the light averaged over every
        direction has that luminance (keeps a scene's exposure while the light takes the room's
        direction and colour); 0 leaves it as the map says. Once, when the room is baked. */
    /** Something standing on a panel, for bakePanelOcclusion: a sphere in the panel's space (x across,
        y out of the panel, z down the panel). A knob is one as tall as it stands, half sunk in the panel if flat. */
    struct OccluderSphere { float x, y, z, r; };

    /** A panel's light map, baked once on the CPU so the shader reads it in one fetch (uOccMap): for every
        point of the panel's face, traced in closed form against the spheres, r = ambient visibility (exact
        sphere occlusion, Quilez) and g = the key light's soft shadow (its core fading past a few widths,
        as a big window's does). RGBA bytes, `w` x `h`, covering x0 .. x0 + width, z0 .. z0 + height.
        `light` is the direction towards the key light in the panel's space. */
    std::vector<unsigned char> bakePanelOcclusion (int w, int h, float x0, float z0, float width, float height,
                                                   const OccluderSphere* spheres, int count, const float light[3]);

    std::array<float, 27> shIrradianceUniforms (const unsigned char* rgba, int width, int height, float range,
                                                float meanLuma = 0.0f) noexcept;

    /** Built-in materials. */
    namespace library
    {
        extern const Material powderCoat;     // textured powder-coated steel. uBaseColor
        extern const Material anodisedPanel;  // brushed panel + RGBA decal (r white print, g outlines, b fields). uParams = decal rect
        extern const Material lacquerPanel;   // glossy painted panel + R8 print. uParams = decal rect, uBaseColor = paint
        extern const Material chrome;         // uParams.x polish, uParams.y > 0.5 brushed rings
        extern const Material plastic;        // uParams.x ridge count, uParams.y ridges below local y; uEmissive = hover lift
        extern const Material woodTable;
        extern const Material walnut;         // oiled walnut, fwidth-antialiased grain. uBaseColor = stain, uParams.x = grain along x
        extern const Material emissive;       // LEDs / lamps / vents. uEmissive, uGlow * uParams.x
        extern const Material recess;         // cavity walls. uParams.x depth, uGlow
        extern const Material printRing;      // alpha-blended printed ring. uParams = (outer radius, _, blend uTex->uTex2, accent)
        extern const Material softShadow;     // analytic rounded-rect shadow on a unit quad
        extern const Material screenOverlay;  // screen-space quads: uParams = (textured, alpha, round, _)
        extern const Material glowSprite;     // additive halo on a unit quad: uBaseColor * uParams.x, uParams.y falloff
        extern const Material valueArc;       // knob value arc on an annulus: uParams = (from angle, to angle, alpha, track alpha)
        extern const Material brushedFace;
        extern const Material meterFace;
        extern const Material coverGlass;
        extern const Material windowLight;
        extern const Material magnifierLens;  // fisheye loupe: uTex = zoomed view; uParams = (fisheye, chroma, _, alpha), uParams2 = (centre px, radius px, _)
    }
}
