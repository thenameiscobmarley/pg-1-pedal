namespace hwk::shaders
{
    const char* const vertex = R"GLSL(
#version 150
in vec3 aPos;
in vec3 aNormal;
in vec2 aUV;

uniform mat4 uModel;
uniform mat4 uViewProj;
// The lighting's slow-changing parts, per vertex (a fraction of the cost per pixel): see the header
uniform vec3  uSH[9];
uniform float uShOn;
uniform mat4  uOccToPanel;
uniform vec4  uOccRect;
uniform vec2  uOccAtlas;
uniform float uOccMapOn;

out vec3 vWorld;
out vec3 vNormal;
out vec2 vUV;
out vec3 vLocal;
out vec3  vShAmb;     // the room's diffuse light for this normal
out vec2  vOccUv;     // where this point is in its panel's light map
out float vOccFlat;   // 1: the panel's face (or flat on it): the map applies
out float vPlateAo;   // what stands on the panel: the plate's own occlusion of it, near it

void main()
{
    vec4 world = uModel * vec4 (aPos, 1.0);
    vWorld  = world.xyz;
    vNormal = mat3 (uModel) * aNormal;
    vUV     = aUV;
    vLocal  = aPos;

    vShAmb = vec3 (0.0);
    if (uShOn > 0.5)
    {
        vec3 n = normalize (vNormal);
        vShAmb = max (vec3 (0.0), uSH[0] + uSH[1] * n.y + uSH[2] * n.z + uSH[3] * n.x
                                + uSH[4] * (n.x * n.y) + uSH[5] * (n.y * n.z) + uSH[6] * (3.0 * n.z * n.z - 1.0)
                                + uSH[7] * (n.x * n.z) + uSH[8] * (n.x * n.x - n.y * n.y));
    }
    vOccUv = vec2 (0.0);
    vOccFlat = 0.0;
    vPlateAo = 1.0;
    if (uOccMapOn > 0.5)
    {
        vec3 p  = (uOccToPanel * world).xyz;
        vec3 pn = normalize (mat3 (uOccToPanel) * vNormal);
        vOccUv = vec2 (clamp ((p.x - uOccRect.x) * uOccRect.z, 0.002, 0.998),
                       uOccAtlas.x + clamp ((p.z - uOccRect.y) * uOccRect.w, 0.01, 0.99) * uOccAtlas.y);
        vOccFlat = smoothstep (0.004, 0.0008, p.y) * smoothstep (0.4, 0.85, pn.y);
        float nearPlate = exp (-max (p.y, 0.0) / 0.012) * (1.0 - vOccFlat);
        vPlateAo = 1.0 - 0.45 * nearPlate * (1.0 - clamp (pn.y, 0.0, 1.0));
    }
    gl_Position = uViewProj * world;
}
)GLSL";

    static const char* const header = R"GLSL(
#version 150
in vec3 vWorld;
in vec3 vNormal;
in vec2 vUV;
in vec3 vLocal;
in vec3  vShAmb;
in vec2  vOccUv;
in float vOccFlat;
in float vPlateAo;

out vec4 fragColor;

uniform vec3  uCamPos;
uniform vec3  uLightDir;
uniform float uTime;
uniform vec2  uViewport;
uniform float uVignette;

uniform vec3  uBaseColor;
uniform vec3  uEmissive;
uniform vec4  uParams;
uniform vec4  uParams2;
uniform vec3  uGlow;

uniform sampler2D uTex;
uniform sampler2D uTex2;
uniform sampler2D uEnv;      // the room to reflect (envColor), when uEnvMix is 1
uniform float uEnvMix;
uniform float uEnvRange;
uniform float uCoat;        // clear coat (0 = none): a wet / lacquered top layer mirroring the room
uniform float uCoatLod;     // how sharp that mirror is (the room map's mip level)
uniform vec4  uBounceGeo;   // one bounce of light (off when uBounceWood is black): x = the side walls' |x|
uniform vec3  uBounceWood;  // (the case cheeks, facing inward), y = the floor's height, z = how far it carries,
uniform vec3  uBounceFloor; // w = the floor's reach; the light they throw back (their colour x how lit they are)
uniform sampler2D uSmudge;  // the coat's smudges (r) and wipe marks (g), tiling; used when uSmudgeOn is 1
uniform float uSmudgeOn;
uniform float uWear[15];   // the surface's three scratches: ends and opacity (wearUniforms, set per draw)
uniform float uShOn;        // 1: ambient light comes from the room itself: uSH, its L2 spherical harmonics
                            // turned into irradiance (shIrradianceUniforms), evaluated per vertex
uniform sampler2D uOccMap;  // a light map per panel, baked on the CPU (bakePanelOcclusion): r = ambient
uniform float uOccMapOn;    // visibility, g = key-light shadow. Read where uOccMapOn is 1: the vertex shader
                            // takes each point into its panel's space (uOccToPanel: world -> x across, y out,
                            // z down), uOccRect = (x0, z0, 1 / width, 1 / height) of the baked area and
                            // uOccAtlas = (v0, v size) of this panel in the map

float gAo = 1.0;           // this pixel's ambient visibility (0 .. 1), worked out in the prelude
float gSpecOcc = 1.0;      // how much of the room its reflection still sees

const vec3 skyCol    = vec3 (0.80, 0.74, 0.80);
const vec3 groundCol = vec3 (0.30, 0.22, 0.23);

/*  What a surface reflects. By default a simple studio worked out here; a plugin can give its own
    room instead, baked into an equirectangular texture (uEnv: u = atan (x, z) around, 0.5 facing +z;
    v = 0 straight up; colour stored as sqrt (c / uEnvRange)) and set uEnvMix to 1. */
vec3 envAnalytic (vec3 r);
vec3 gR = vec3 (0.0);     // this pixel's reflection vector, and where it falls on the room map: worked out once
vec2 gEnvUv = vec2 (0.5); // in the prelude (an atan and an acos), shared by the material and the clear coat
vec2 envUv (vec3 r) { return vec2 (atan (r.x, r.z) * 0.15915494 + 0.5, acos (clamp (r.y, -1.0, 1.0)) * 0.31830989); }
vec3 envColorLod (vec3 r, float lod)
{
    if (uEnvMix > 0.5)
    {
        vec2 uv = all (equal (r, gR)) ? gEnvUv : envUv (r);
        vec3 e = textureLod (uEnv, uv, lod).rgb;
        return e * e * uEnvRange * gSpecOcc;
    }
    return envAnalytic (r) * gSpecOcc;
}
vec3 envColor (vec3 r) { return envColorLod (r, 0.5); }   // (sharp: a wet world, no haze)

vec3 envAnalytic (vec3 r)
{
    float t = clamp (r.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 c = mix (groundCol, skyCol, smoothstep (0.30, 0.75, t));
    c += vec3 (0.95, 0.85, 0.92) * exp (-pow ((r.y - 0.05) * 5.0, 2.0)) * 0.35;
    c += vec3 (1.0, 0.93, 0.97) * pow (max (dot (r, normalize (vec3 (-0.15, 0.30, 1.0))), 0.0), 4.0) * 0.55;
    float wall = smoothstep (0.15, 0.85, r.z);
    float strips = pow (0.5 + 0.5 * cos (r.x * 9.0), 8.0);
    c += vec3 (0.88, 0.80, 0.86) * wall * (0.35 + 0.55 * strips);
    c += vec3 (1.0, 0.95, 0.97) * pow (max (dot (r, normalize (vec3 (-0.45, 0.75, 0.55))), 0.0), 24.0) * 1.1;
    c += vec3 (1.0, 0.82, 0.90) * pow (max (dot (r, normalize (vec3 (0.70, 0.35, 0.60))), 0.0), 40.0) * 0.5;
    return c;
}

float rrectSdf (vec2 p, vec2 halfSize, float r)
{
    vec2 q = abs (p) - halfSize + r;
    return length (max (q, 0.0)) + min (max (q.x, q.y), 0.0) - r;
}

/*  Anti-aliasing for per-cell noise (hash12 (floor (x))): when a pixel covers n cells, what it
    should show is their average, whose spread shrinks as 1 / sqrt (n). Sampling one cell per pixel
    instead gives full-contrast noise that crawls as the camera moves. `cells` is the noise
    coordinate's per-pixel change, e.g. fwidth (p * 3000.0). */
float noiseAA (float cells)
{
    return inversesqrt (max (1.0, cells));
}

float hash12 (vec2 p)
{
    vec3 p3 = fract (vec3 (p.xyx) * 0.1031);
    p3 += dot (p3, p3.yzx + 33.33);
    return fract ((p3.x + p3.y) * p3.z);
}

float valueNoise (vec2 p)
{
    vec2 i = floor (p), f = fract (p);
    f = f * f * (3.0 - 2.0 * f);
    return mix (mix (hash12 (i), hash12 (i + vec2 (1, 0)), f.x),
                mix (hash12 (i + vec2 (0, 1)), hash12 (i + vec2 (1, 1)), f.x), f.y);
}

/*  Wear: three scratches per surface, each with its own angle, length and opacity - the
    marks a unit picks up in a rack over a few years, not a sandblasted finish.
    `seed` varies them from unit to unit. */
float scratchLine (vec2 p, vec2 a, vec2 b, float width)
{
    vec2 pa = p - a, ba = b - a;
    float h = clamp (dot (pa, ba) / max (dot (ba, ba), 1.0e-6), 0.0, 1.0);
    float d = length (pa - ba * h);
    // taper the ends, and let the mark fade along its length
    float taper = smoothstep (0.0, 0.12, h) * smoothstep (1.0, 0.86, h);
    return smoothstep (width, 0.0, d) * taper;
}

float wearMarks (vec2 p, float seed, float scale)
{
    // The scratches' ends and opacity are worked out once per surface on the CPU (wearUniforms):
    // per pixel only the distance to each is left
    float total = 0.0;
    for (int i = 0; i < 3; ++i)
    {
        vec2 a = vec2 (uWear[i * 5], uWear[i * 5 + 1]);
        vec2 b = vec2 (uWear[i * 5 + 2], uWear[i * 5 + 3]);
        total = max (total, scratchLine (p, a, b, 0.0016 * scale) * uWear[i * 5 + 4]);
    }
    return total;
}
float patina (vec2 p)
{
    return valueNoise (p * 1.7) * 0.6 + valueNoise (p * 5.3) * 0.4;
}

/*  Light from a window off to the upper left: three broad shafts of warm sun with soft, wide edges
    (a big window, a little haze), falling across the rack without cutting it into bars. */
float windowBeam (vec3 world)
{
    float across = dot (world, normalize (vec3 (0.80, 0.56, 0.20)));

    float b = 0.0;
    b += smoothstep (1.95, 1.05, abs (across - 0.35));
    b += 0.80 * smoothstep (1.35, 0.55, abs (across + 1.75));
    b += 0.65 * smoothstep (1.25, 0.45, abs (across - 2.85));

    // Haze: light scattered around the shafts
    float spill = 0.05 * smoothstep (3.2, 1.0, abs (across - 0.35));   // (clear air: little haze)
    return clamp (b + spill, 0.0, 1.6);
}
)GLSL";

    static const char* const prelude = R"GLSL(
void main()
{
    vec3 N = normalize (vNormal + vec3 (0.0, 1.0e-6, 0.0));
    vec3 V = normalize (uCamPos - vWorld);
    vec3 L = normalize (uLightDir);
    vec3 H = normalize (L + V);

    float ndl  = max (dot (N, L), 0.0);
    float wrap = max ((dot (N, L) + 0.4) / 1.4, 0.0);
    float ndv  = clamp (dot (N, V), 0.0, 1.0);
    float ndh  = max (dot (N, H), 0.0);
    float facing = 1.0 - ndv;

    float beam = windowBeam (vWorld);
    // In the shaft: bright, warm, directional. Out of it: the room's bounce - softer and a little
    // cooler, but enough to read every panel by (a lamp and the walnut around it warm it up)
    vec3 lightCol = mix (vec3 (0.62, 0.58, 0.60), vec3 (2.10, 1.88, 1.58), clamp (beam, 0.0, 1.0));
    vec3 amb  = mix (groundCol, skyCol, N.y * 0.5 + 0.5) * (0.58 + 0.42 * clamp (beam, 0.0, 1.0));
    float fill = max (dot (N, normalize (vec3 (0.75, 0.35, 0.9))), 0.0) * (0.12 + 0.16 * clamp (beam, 0.0, 1.0));
    vec3 R = reflect (-V, N);
    // The room's own light, from all around (per vertex), and the baked light map: ambient occlusion and
    // the key light's soft shadow, traced once on the CPU against what stands on the panel - one fetch
    if (uShOn > 0.5)
        amb = vShAmb * (0.58 + 0.42 * clamp (beam, 0.0, 1.0));
    float keyVis = 1.0;
    if (uOccMapOn > 0.5)
    {
        vec2 m = textureLod (uOccMap, vOccUv, 0.0).rg;
        gAo = mix (1.0, m.r, vOccFlat) * vPlateAo;
        keyVis = mix (1.0, m.g, vOccFlat);
    }
    gAo = clamp (gAo, 0.0, 1.0);
    if (gAo < 0.999)
    {
        // Reflections: the occlusion's share (Lagarde 2014's specular occlusion, in a cheap fit)
        gSpecOcc = clamp (gAo * (1.0 + 0.6 * ndv * (1.0 - gAo)), 0.0, 1.0);
        amb *= gAo + clamp (uBaseColor, 0.0, 1.0) * (gAo * (1.0 - gAo));   // (light bounced back by bright surfaces)
        fill *= gAo;
    }
    lightCol *= mix (1.0, keyVis, 0.85);   // (the window is big: its shadows never go fully black)
    if (uBounceWood.r + uBounceWood.g + uBounceWood.b > 0.0)
    {
        // One bounce, in closed form: a big lit surface nearby throws its colour onto whatever faces it,
        // fading with distance. The two walnut cheeks warm the units' edges and knob flanks facing them;
        // the floor warms what faces down. No rays, no extra passes: a few multiplies per pixel.
        float dl = max (vWorld.x + uBounceGeo.x, 0.0), dr = max (uBounceGeo.x - vWorld.x, 0.0);
        float side = max (N.x, 0.0) * exp (-dl / uBounceGeo.z) + max (-N.x, 0.0) * exp (-dr / uBounceGeo.z);
        float floorB = max (-N.y, 0.0) * exp (-max (vWorld.y - uBounceGeo.y, 0.0) / uBounceGeo.w);
        amb += (uBounceWood * side + uBounceFloor * floorB) * gAo;
    }
    if (uEnvMix > 0.5)
    {
        gR = R;
        gEnvUv = envUv (R);
    }

    vec3 col = vec3 (0.0);
    float alpha = 1.0;
)GLSL";

    static const char* const postProcess = R"GLSL(
    if (uCoat > 0.0)
    {
        // Clear coat: a smooth wet / lacquered layer over whatever is under it. It mirrors the room
        // sharply, faintly face-on and strongly at grazing angles (Schlick's Fresnel, n = 1.5), and
        // the key light shows in it as a tight, bright glint - what makes a surface look wet.
        float fc = 0.04 + 0.96 * pow (1.0 - ndv, 5.0);
        // Real lacquer isn't perfect: faint smudges and wipe marks blur the mirror here and there, so
        // reflections break up as they cross a surface instead of sliding over it like a sticker
        vec3 wp = vWorld * 7.0;
        float smudge, wipe;
        if (uSmudgeOn > 0.5)
        {
            vec2 sw = texture (uSmudge, (wp.xy + wp.z * 0.7) * (1.0 / 16.0)).rg;   // baked: one fetch
            smudge = sw.r;
            wipe = sw.g;
        }
        else
        {
            smudge = valueNoise (wp.xy + wp.z * 0.7) * 0.6 + valueNoise (wp.xy * 3.1 - wp.z) * 0.4;
            wipe = valueNoise (vec2 (wp.x * 0.35 + wp.y * 1.9, wp.z * 2.0));
        }
        // (only a trace of it now: the coat reads as still water, a mirror with no haze)
        float blur = uCoatLod + 0.30 * smoothstep (0.6, 0.95, smudge) + 0.12 * smoothstep (0.7, 0.98, wipe);
        col = col * (1.0 - fc * uCoat) + envColorLod (R, blur) * (fc * uCoat) * (1.0 - 0.04 * smoothstep (0.6, 0.95, smudge));
        col += lightCol * (pow (ndh, 1400.0) * 5.0 + pow (ndh, 300.0) * 0.35) * uCoat;
    }
    col = col * (1.0 + col / 4.0) / (1.0 + col);
    vec2 sp = gl_FragCoord.xy / uViewport - 0.5;
    col *= 1.0 - uVignette * 0.34 * pow (length (sp) * 1.25, 2.4);
    col += (hash12 (gl_FragCoord.xy) - 0.5) / 255.0;
)GLSL";

    namespace
    {
        // The shader's hash12, in the same float arithmetic
        float fractF (float x) noexcept { return x - std::floor (x); }
        float hash12 (float x, float y) noexcept
        {
            float p0 = fractF (x * 0.1031f), p1 = fractF (y * 0.1031f), p2 = fractF (x * 0.1031f);
            const float d = p0 * (p1 + 33.33f) + p1 * (p2 + 33.33f) + p2 * (p0 + 33.33f);
            p0 += d; p1 += d; p2 += d;
            return fractF ((p0 + p1) * p2);
        }
    }

    std::vector<unsigned char> bakePanelOcclusion (int w, int h, float x0, float z0, float width, float height,
                                                   const OccluderSphere* spheres, int count, const float light[3])
    {
        // Exact sphere occlusion and a sphere's soft shadow (Quilez), for a point on the face (normal +y)
        auto occlusion = [] (float px, float py, float pz, const OccluderSphere& s)
        {
            const float dx = s.x - px, dy = s.y - py, dz = s.z - pz;
            const float l = std::sqrt (dx * dx + dy * dy + dz * dz);
            const float h = l / s.r;
            if (h < 1.02f) return 0.6f;   // right under it (hidden; a bezel or button top lying there: in shade)
            const float nl = dy / l, h2 = h * h, k2 = 1.0f - h2 * nl * nl;
            float res = std::max (0.0f, nl) / h2;
            if (k2 > 0.001f)
            {
                res = nl * std::acos (std::clamp (-nl * std::sqrt ((h2 - 1.0f) / std::max (1.0f - nl * nl, 1.0e-4f)), -1.0f, 1.0f))
                      - std::sqrt (k2 * (h2 - 1.0f));
                res = res / h2 + std::atan (std::sqrt (k2 / (h2 - 1.0f)));
                res *= 0.31830989f;
            }
            return std::clamp (res, 0.0f, 1.0f);
        };
        auto softShadow = [] (float ox, float oy, float oz, const float* rd, const OccluderSphere& s, float k)
        {
            const float cx = ox - s.x, cy = oy - s.y, cz = oz - s.z;
            const float b = cx * rd[0] + cy * rd[1] + cz * rd[2];
            const float c = cx * cx + cy * cy + cz * cz - s.r * s.r;
            const float hh = b * b - c;
            const float d = std::sqrt (std::max (0.0f, s.r * s.r - hh)) - s.r;
            const float t = -b - std::sqrt (std::max (hh, 0.0f));
            if (t < 0.0f) return 1.0f;
            auto smooth = [] (float e0, float e1, float x) { const float q = std::clamp ((x - e0) / (e1 - e0), 0.0f, 1.0f); return q * q * (3.0f - 2.0f * q); };
            const float fade = smooth (3.0f * s.r, 0.8f * s.r, t);
            const float sh = t > 1.0e-5f ? smooth (0.0f, 1.0f, 2.5f * k * d / t) : 0.0f;
            return 1.0f + (sh - 1.0f) * fade;
        };

        std::vector<unsigned char> out ((size_t) (w * h * 4), 255);
        for (int j = 0; j < h; ++j)
        {
            const float pz = z0 + height * ((float) j + 0.5f) / (float) h;
            for (int i = 0; i < w; ++i)
            {
                const float px = x0 + width * ((float) i + 0.5f) / (float) w, py = 0.0005f;
                float ao = 1.0f, key = 1.0f;
                for (int n = 0; n < count; ++n)
                {
                    const auto& s = spheres[n];
                    const float dx = s.x - px, dz = s.z - pz;
                    if (dx * dx + dz * dz > 49.0f * s.r * s.r)
                        continue;   // beyond seven radii: nothing to see
                    ao *= 1.0f - occlusion (px, py, pz, s);
                    key = std::min (key, softShadow (px, py, pz, light, s, 3.0f));
                }
                auto* o = out.data() + ((size_t) j * (size_t) w + (size_t) i) * 4;
                o[0] = (unsigned char) std::lround (std::clamp (ao, 0.0f, 1.0f) * 255.0f);
                o[1] = (unsigned char) std::lround (std::clamp (key, 0.0f, 1.0f) * 255.0f);
            }
        }
        return out;
    }

    std::array<float, 27> shIrradianceUniforms (const unsigned char* rgba, int width, int height, float range,
                                                float meanLuma) noexcept
    {
        // Real spherical harmonics up to l = 2 in the shader's order (shIrradiance): their constants,
        // and the cosine lobe's per band (pi, 2 pi / 3, pi / 4), over pi: irradiance as radiance
        constexpr float k[9] { 0.282095f, 0.488603f, 0.488603f, 0.488603f, 1.092548f, 1.092548f, 0.315392f, 1.092548f, 0.546274f };
        constexpr float band[9] { 1.0f, 2.0f / 3.0f, 2.0f / 3.0f, 2.0f / 3.0f, 0.25f, 0.25f, 0.25f, 0.25f, 0.25f };
        constexpr float pi = 3.14159265f;
        double L[9][3] {};
        for (int j = 0; j < height; ++j)
        {
            const float theta = pi * ((float) j + 0.5f) / (float) height;   // 0 = straight up
            const float y = std::cos (theta), st = std::sin (theta);
            const double dOmega = (2.0 * pi / width) * (pi / height) * st;
            for (int i = 0; i < width; ++i)
            {
                const float phi = 2.0f * pi * (((float) i + 0.5f) / (float) width - 0.5f);
                const float x = st * std::sin (phi), z = st * std::cos (phi);
                const float p[9] { 1.0f, y, z, x, x * y, y * z, 3.0f * z * z - 1.0f, x * z, x * x - y * y };
                const unsigned char* px = rgba + ((size_t) j * (size_t) width + (size_t) i) * 4;
                float c[3];
                for (int ch = 0; ch < 3; ++ch) { const float e = px[ch] / 255.0f; c[ch] = e * e * range; }
                for (int b = 0; b < 9; ++b)
                    for (int ch = 0; ch < 3; ++ch)
                        L[b][ch] += (double) c[ch] * k[b] * p[b] * dOmega;
            }
        }
        std::array<float, 27> out {};
        for (int b = 0; b < 9; ++b)
            for (int ch = 0; ch < 3; ++ch)
                out[(size_t) (b * 3 + ch)] = (float) L[b][ch] * band[b] * k[b];
        if (meanLuma > 0.0f)
        {
            // The average over every direction is the constant term alone (the others average to zero)
            const float luma = 0.2126f * out[0] + 0.7152f * out[1] + 0.0722f * out[2];
            if (luma > 1.0e-6f)
                for (auto& v : out)
                    v *= meanLuma / luma;
        }
        return out;
    }

    std::array<float, 15> wearUniforms (float seed, float scale) noexcept
    {
        std::array<float, 15> u {};
        for (int i = 0; i < 3; ++i)
        {
            const float fi = (float) i;
            const float r1 = hash12 (seed * 7.1f + fi, 3.7f), r2 = hash12 (seed * 3.3f + fi, 9.1f);
            const float r3 = hash12 (seed * 5.9f + fi, 1.3f), r4 = hash12 (seed * 2.7f + fi, 6.5f);
            const float ax = (r1 - 0.5f) * 1.8f * scale, ay = (r2 - 0.5f) * 1.8f * scale;
            const float ang = (r3 - 0.5f) * 0.9f;                 // shallow angles, as if wiped
            const float len = (0.18f + 0.55f * r4) * scale;
            const auto k = (size_t) i * 5;
            u[k] = ax;
            u[k + 1] = ay;
            u[k + 2] = ax + std::cos (ang) * len;
            u[k + 3] = ay + std::sin (ang) * 0.35f * len;
            u[k + 4] = 0.25f + 0.75f * hash12 (seed + fi * 11.0f, 17.0f);
        }
        return u;
    }

    juce::String fragmentSource (const Material& m)
    {
        juce::String s;
        s << header << "\n// ---- " << m.name << " declarations\n" << m.declarations << "\n" << prelude
          << "\n// ---- " << m.name << "\n" << m.body << "\n";
        if (m.postProcess)
            s << postProcess;
        s << "    fragColor = vec4 (col, alpha);\n}\n";
        return s;
    }

    namespace library
    {
        const Material powderCoat { "powderCoat", R"GLSL(
    vec2 grainAt = vWorld.xz * 900.0 + vWorld.y * 700.0;
    float grain = (hash12 (floor (grainAt)) - 0.5) * noiseAA (length (fwidth (grainAt)));
    float aged = patina (vWorld.xy * 2.2 + vWorld.z * 0.3);
    vec3 albedo = uBaseColor * (1.0 + grain * 0.10) * (0.92 + 0.14 * aged);
    col  = albedo * (amb * 0.55 + wrap * lightCol * 0.70 + fill);
    col += lightCol * (pow (ndh, 28.0) * 0.10 + pow (ndh, 120.0) * 0.18) * (0.75 + 0.35 * aged);
    col += envColor (R) * (0.02 + 0.18 * pow (facing, 4.0));
)GLSL" };

        const Material anodisedPanel { "anodisedPanel", R"GLSL(
    vec2 p = vLocal.xz;
    vec4 d = texture (uTex, (p - uParams.xy) / uParams.zw);
    float brush = (hash12 (vec2 (floor (p.y * 2600.0), floor (p.x * 40.0))) - 0.5) * noiseAA (fwidth (p.y * 2600.0));
    vec3 panel = (uParams2.x > 0.0 ? uBaseColor : vec3 (0.085, 0.089, 0.098)) * (1.0 + brush * 0.10);   // uParams2.x > 0: its own shade
    panel = mix (panel, vec3 (0.118, 0.123, 0.134), d.b * 0.85);
    vec3 albedo = mix (panel, vec3 (0.46, 0.48, 0.52), d.g);
    if (uParams2.z > 0.5)
        albedo = mix (albedo, vec3 (0.10, 0.32, 0.78), d.a);   // uParams2.z: this print has blue pinstripes in its alpha
    albedo = mix (albedo, vec3 (0.90, 0.91, 0.92), d.r);
    float wear = wearMarks (p, uParams2.w, 1.0);
    float edgeWear = smoothstep (0.72, 1.0, abs (p.y) / max (uParams.w * 0.5, 0.001)) * 0.5
                   + smoothstep (0.86, 1.0, abs (p.x) / max (uParams.z * 0.5, 0.001)) * 0.5;
    albedo = mix (albedo, albedo * 1.35 + vec3 (0.02), clamp (wear * 0.55 + edgeWear * 0.10, 0.0, 0.35));
    col  = albedo * (amb * 0.75 + wrap * lightCol * 0.85 + fill);
    float satin = pow (max (dot (R, normalize (vec3 (-0.15, 0.30, 1.0))), 0.0), 18.0);
    col += vec3 (0.85, 0.88, 0.95) * (satin * 0.10 + pow (ndh, 60.0) * 0.20 + pow (ndh, 8.0) * 0.03) * (1.0 - d.r * 0.5);
    col += vec3 (0.9, 0.92, 1.0) * wear * pow (ndh, 12.0) * 0.30;
    col += envColor (R) * (0.015 + 0.20 * pow (facing, 5.0));
)GLSL" };

        const Material lacquerPanel { "lacquerPanel", R"GLSL(
    vec2 p = vLocal.xz;
    float print = texture (uTex, (p - uParams.xy) / uParams.zw).r;
    float flake = (hash12 (floor (p * 1400.0)) - 0.5) * noiseAA (length (fwidth (p * 1400.0)));
    // uParams2.y > 0.5: dark ink (a light-painted panel); uParams2.z: hammertone - the dimpled, mottled
    // finish of old broadcast gear: slow bumps in the paint that shade and catch the light
    vec3 ink = uParams2.y > 0.5 ? vec3 (0.045, 0.045, 0.05) : vec3 (0.93, 0.92, 0.95);
    float hammer = 0.0;
    if (uParams2.z > 0.0)
    {
        vec2 hp = p * 55.0;
        float hAA = clamp (1.0 - length (fwidth (hp)) * 0.35, 0.0, 1.0);
        hammer = (valueNoise (hp) * 0.65 + valueNoise (hp * 2.3 + 7.1) * 0.35 - 0.5) * uParams2.z * hAA;
    }
    vec3 albedo = mix (uBaseColor * (1.0 + flake * 0.06) * (1.0 + hammer * 0.55), ink, print);
    float wear = wearMarks (p, uParams2.w + 3.0, 1.0);
    float dust = patina (p * 3.0) * 0.5 + 0.5;
    albedo = mix (albedo, albedo * 1.25 + vec3 (0.015), wear * (uParams2.z > 0.0 ? 0.12 : 0.40));
    col  = albedo * (amb * 0.70 + wrap * lightCol * 0.85 + fill) * (0.95 + 0.08 * dust * (uParams2.z > 0.0 ? 0.25 : 1.0));   // a clean hammertone: barely any grime
    float coat = (1.0 - print * 0.4) * (1.0 - wear * 0.35);
    col += lightCol * (pow (ndh, 180.0) * 0.55 + pow (ndh, 24.0) * 0.06) * coat * (1.0 + hammer * 1.6);
    col += envColor (R) * (0.04 + 0.32 * pow (facing, 5.0)) * coat;
    col += uBaseColor * pow (facing, 3.0) * 0.12 * coat;   // the paint's own colour at grazing angles
    col += vec3 (1.0, 0.97, 0.92) * wear * pow (ndh, 20.0) * 0.25;
)GLSL" };

        const Material chrome { "chrome", R"GLSL(
    float polish = uParams.x;
    float fres = mix (0.55, 1.0, pow (facing, 5.0));
    col  = uBaseColor * (amb * 0.18 + ndl * lightCol * 0.25) * (1.0 - polish * 0.6);
    col += envColor (R) * mix (uBaseColor, vec3 (1.0), fres * 0.5) * mix (0.55, 0.95, polish) * fres;
    col += lightCol * pow (ndh, mix (40.0, 220.0, polish)) * mix (0.45, 1.5, polish);
    col += vec3 (0.80, 0.84, 0.92) * pow (facing, 3.0) * 0.08;
    if (uParams.y > 0.5)
    {
        float rings = length (vLocal.xz) * 700.0;
        col *= 1.0 + 0.07 * sin (rings) * clamp (1.0 - fwidth (rings) * 0.6, 0.0, 1.0);
    }
    col += uEmissive;
)GLSL" };

        const Material plastic { "plastic", R"GLSL(
    float shade = 1.0;
    if (uParams.x > 0.0 && vLocal.y < uParams.y && vLocal.y > 0.02)
    {
        // Grip ridges, anti-aliased: the ridge phase's change per pixel (from the radius, not from
        // atan, which jumps at its seam) widens the edge, and ridges finer than ~a pixel fade to
        // their average instead of stair-stepping into moire.
        float radius = max (length (vLocal.xz), 1.0e-4);
        float perPixel = uParams.x * length (fwidth (vLocal.xz)) / radius;
        float soft = 0.6 + perPixel;
        float ridge = smoothstep (-soft, soft, sin (atan (vLocal.x, vLocal.z) * uParams.x));
        ridge = mix (0.5, ridge, clamp (1.6 - perPixel * 0.55, 0.0, 1.0));
        shade = 0.35 + 0.65 * ridge;
    }
    col  = uBaseColor * (amb * 0.60 + wrap * lightCol * 0.80 + fill);
    // Satin, like moulded phenolic / ABS knobs: a broad soft sheen, not a hard gloss dot
    col += lightCol * (pow (ndh, 26.0) * 0.13 + pow (ndh, 6.0) * 0.045) * shade;
    col += envColor (R) * (0.03 + 0.13 * pow (facing, 4.0)) * shade;
    col += uEmissive;
)GLSL" };

        const Material walnut { "walnut", R"GLSL(
    // Oiled walnut. uBaseColor = the wood's mid tone (stain), uParams.x = grain direction
    // (0: along the case's arc, 1: across, along x). Figure from rings bent by slow noise, open pores
    // along the grain; every stripe fades to its average once it is finer than a pixel (no moire).
    float along  = mix (vWorld.y + vWorld.z * 0.35, vWorld.x, uParams.x);
    float across = mix (vWorld.x + vWorld.z * 0.80, vWorld.y + vWorld.z * 0.50, uParams.x);
    float warp = valueNoise (vec2 (along * 1.3, across * 3.0)) * 2.0 + valueNoise (vec2 (along * 0.45, across * 9.0));
    float ring = across * 36.0 + warp * 3.4;
    float ringAA = clamp (1.0 - fwidth (ring) * 0.55, 0.0, 1.0);
    float grain = mix (0.5, 0.5 + 0.5 * sin (ring), ringAA);
    float fine = mix (0.5, 0.5 + 0.5 * sin (ring * 5.3 + warp * 7.0), clamp (1.0 - fwidth (ring * 5.3) * 0.6, 0.0, 1.0));
    vec2 poreAt = vec2 (along * 60.0, across * 700.0);
    float pores = (valueNoise (poreAt) - 0.5) * noiseAA (length (fwidth (poreAt)));
    float figure = patina (vec2 (along * 0.8, across * 2.5));

    vec3 dark = uBaseColor * 0.55, light = uBaseColor * 1.30;
    vec3 albedo = mix (dark, light, clamp (0.15 + grain * 0.55 + fine * 0.15 + figure * 0.25, 0.0, 1.0));
    albedo *= 1.0 + pores * 0.35;

    col  = albedo * (amb * 0.55 + wrap * lightCol * 0.78 + fill);
    // Oil finish: a soft sheen, a little sharper highlight, the room in it at grazing angles
    col += lightCol * (pow (ndh, 70.0) * 0.22 + pow (ndh, 12.0) * 0.035) * (0.8 + 0.4 * grain);
    col += envColor (R) * (0.015 + 0.20 * pow (facing, 5.0));
)GLSL" };

        const Material woodTable { "woodTable", R"GLSL(
    vec2 p = vWorld.xz;
    float grain = sin (p.x * 6.0 + sin (p.y * 1.2 + p.x * 0.35) * 2.4 + sin (p.y * 0.29) * 4.0);
    float fine  = sin (p.x * 58.0 + sin (p.y * 2.9) * 3.0) * 0.5 + 0.5;
    vec3 albedo = mix (vec3 (0.16, 0.095, 0.075), vec3 (0.30, 0.18, 0.13), 0.5 + 0.3 * grain) * (0.90 + 0.10 * fine);
    col  = albedo * (amb * 0.5 + ndl * lightCol * 0.75);
    col += lightCol * pow (ndh, 24.0) * 0.08;
    col *= mix (1.0, 0.15, smoothstep (3.5, 10.0, length (p - vec2 (0.0, 0.2))));
)GLSL" };

        const Material emissive { "emissive", R"GLSL(
    /*  Lit plastic: the body glows, the curved edge of the lens catches more of it (light
        travelling along the moulding), and there is a hard specular dot where the room
        reflects off the dome. That combination is what makes an LED read as an LED. */
    float edge = pow (1.0 - ndv, 2.2);
    float core = pow (ndv, 2.0);

    col  = uBaseColor + uEmissive * (0.55 + 0.75 * core + 0.85 * edge);
    col += uEmissive * uEmissive * 0.45 * core;                       // saturated centre
    col += uGlow * (1.6 + 0.5 * sin (vLocal.z * 14.0 - uTime * 2.4)) * uParams.x;
    col += vec3 (1.0) * pow (ndh, 90.0) * 0.55;                       // the dome's highlight
    col += vec3 (0.9, 0.95, 1.0) * pow (ndh, 18.0) * 0.06;
)GLSL" };

        const Material recess { "recess", R"GLSL(
    float depthT = clamp ((vLocal.y + uParams.x) / uParams.x, 0.0, 1.0);
    col  = uBaseColor * (amb * 0.55 + wrap * 0.45) * (0.30 + 0.70 * depthT);
    col += uGlow * (1.0 - depthT) * 1.5;
)GLSL" };

        const Material printRing { "printRing", R"GLSL(
    vec2 ringUv = vLocal.xz / (2.0 * uParams.x) + 0.5;
    float ink = mix (texture (uTex, ringUv).r, texture (uTex2, ringUv).r, uParams.z);
    vec3 inkCol = mix (vec3 (0.90, 0.91, 0.92), vec3 (1.0, 0.78, 0.30), uParams.w);
    col = inkCol * (amb * 0.75 + wrap * lightCol * 0.85 + fill);
    alpha = ink;
)GLSL" };

        const Material softShadow { "softShadow", R"GLSL(
    vec2 p = vLocal.xz * uParams.xy;
    float sd = rrectSdf (p, uParams.zw, uParams2.x);
    alpha = (1.0 - smoothstep (-uParams2.y, uParams2.y, sd)) * uParams2.z;
    col = vec3 (0.03, 0.0, 0.015);
)GLSL" };

        const Material screenOverlay { "screenOverlay", R"GLSL(
    if (uParams.x > 0.5)
    {
        vec4 t = texture (uTex, vUV);
        col = t.rgb;
        alpha = t.a * uParams.y;
    }
    else
    {
        col = uBaseColor;
        alpha = uParams.y;
        if (uParams.z > 0.5)
        {
            float r = length (vUV - 0.5);
            alpha *= 1.0 - smoothstep (0.36, 0.5, r);
            if (uParams.w > 0.5)          // ring: hollow, for backing something translucent
                alpha *= smoothstep (0.26, 0.42, r);
        }
    }
)GLSL", "", false };

        const Material glowSprite { "glowSprite", R"GLSL(
    /*  The light an indicator throws onto the panel around it: a tight core, a wide soft
        bloom, and a faint ring where a real lens throws its caustic. Additive. */
    float r2 = dot (vLocal.xz, vLocal.xz);
    float r = sqrt (r2);

    float core  = exp (-r2 * uParams.y * 3.4);
    float bloom = exp (-r2 * uParams.y * 0.55);
    float ring  = exp (-pow ((r - 0.42) * 7.0, 2.0)) * 0.22;

    float falloff = 1.0 - smoothstep (0.72, 1.0, r2);
    col = uBaseColor * (1.0 + 1.6 * core);
    alpha = uParams.x * (0.55 * core + 0.55 * bloom + ring) * falloff;
)GLSL", "", false };

        const Material valueArc { "valueArc", R"GLSL(
    // Angle clockwise from -z (the knob's zero direction)
    float a = atan (vLocal.x, -vLocal.z);
    float from = min (uParams.x, uParams.y), to = max (uParams.x, uParams.y);
    float aa = fwidth (a) * 1.5;
    float lit = smoothstep (from - aa, from + aa, a) * (1.0 - smoothstep (to - aa, to + aa, a));
    float track = step (-2.36, a) * step (a, 2.36);
    col = uBaseColor * (1.0 + 0.6 * lit);
    alpha = uParams.z * (lit + uParams.w * track * (1.0 - lit));
)GLSL", "", false };

        const Material brushedFace { "brushedFace", R"GLSL(
    /*  Brushed aluminium faceplate with engraved print: the ink sits *into* the metal, so it
        darkens and catches a different highlight rather than sitting on top like paint.
        uParams = (originX, originZ, width, height) mapping vLocal.xz into the decal. */
    vec2 p = vLocal.xz;
    float print = texture (uTex, (p - uParams.xy) / uParams.zw).r;

    // Horizontal brush, fine and directional, with a slow variation across the plate
    float brush = (hash12 (vec2 (floor (p.y * 3000.0), floor (p.x * 26.0))) - 0.5) * noiseAA (fwidth (p.y * 3000.0))
                + (hash12 (vec2 (floor (p.y * 700.0), floor (p.x * 9.0))) - 0.5) * 0.6 * noiseAA (fwidth (p.y * 700.0));
    float wear = wearMarks (p, uParams2.w + 7.0, 1.0);
    float aged = patina (p * 2.4);

    vec3 metal = uBaseColor * (1.0 + brush * 0.085) * (0.96 + 0.07 * aged);
    metal = mix (metal, metal * 1.22 + vec3 (0.02), wear * 0.5);

    // Engraving: darker, slightly recessed, and it kills the specular
    vec3 albedo = mix (metal, metal * 0.24 + vec3 (0.012, 0.012, 0.014), print);
    float engraved = 1.0 - print * 0.85;

    col  = albedo * (amb * 0.62 + wrap * lightCol * 0.80 + fill);
    float anis = pow (max (dot (R, normalize (vec3 (-0.25, 0.42, 0.87))), 0.0), 26.0);
    col += vec3 (0.92, 0.94, 1.0) * anis * 0.30 * engraved;
    col += lightCol * (pow (ndh, 90.0) * 0.30 + pow (ndh, 14.0) * 0.05) * engraved;
    col += envColor (R) * (0.05 + 0.30 * pow (facing, 4.0)) * engraved;
    col += vec3 (1.0, 0.98, 0.94) * wear * pow (ndh, 24.0) * 0.28;
)GLSL" };

        const Material meterFace { "meterFace", R"GLSL(
    /*  A printed VU face behind glass: cream card, a printed arc scale from the texture, a red
        zone at the top of the scale, and the shadow the needle casts onto the card.
        uParams = (reading 0..1, lamp, redZoneStart, _) */
    vec2 uv = vUV;
    float lamp = uParams.y;

    // Card: warm, slightly uneven, darker toward the corners the way a real dial ages
    float grain = valueNoise (uv * 90.0) * 0.5 + valueNoise (uv * 240.0) * 0.5;
    // (uBaseColor tints the card: (0.92, 0.89, 0.80) is the plain cream; an amber-lit dial passes its own)
    vec3 card = vec3 (0.94, 0.90, 0.78) * (uBaseColor / vec3 (0.92, 0.89, 0.80)) * (0.94 + 0.10 * grain);
    card *= 1.0 - 0.22 * smoothstep (0.45, 1.05, length ((uv - 0.5) * vec2 (1.35, 1.8)));
    card = mix (card * 0.93, card, smoothstep (0.0, 0.35, uv.y));

    float print = texture (uTex, uv).r;
    vec3 ink = vec3 (0.10, 0.10, 0.11);

    // Red zone, printed over the top right of the arc like a real VU
    float redZone = texture (uTex, uv).g;
    vec3 col2 = mix (card, vec3 (0.72, 0.10, 0.08), redZone * 0.85);
    col2 = mix (col2, ink, print);

    // Backlight: these meters are lit from behind the card, brightest near the top
    vec3 lit = col2 * (0.30 + 0.95 * lamp * (0.72 + 0.28 * (1.0 - uv.y)));
    lit += vec3 (1.0, 0.92, 0.72) * lamp * 0.10 * (1.0 - smoothstep (0.0, 0.8, length (uv - vec2 (0.5, 0.15))));

    col = lit * (amb * 0.30 + wrap * lightCol * 0.55 + 0.35 + fill);
)GLSL" };

        const Material coverGlass { "coverGlass", R"GLSL(
    /*  The glass over a meter: mostly transparent, with a reflection of the room, a bright
        streak where the window catches it, and a darker frame shadow at the edges. */
    vec2 uv = vUV;
    vec3 refl = envColor (R);
    float sheen = pow (max (dot (R, normalize (vec3 (-0.45, 0.60, 0.65))), 0.0), 6.0);
    float streak = exp (-pow ((uv.x * 0.8 + uv.y * 1.5 - 0.95) * 3.4, 2.0));

    col  = refl * (0.10 + 0.32 * pow (facing, 3.0));
    col += vec3 (1.0, 0.98, 0.95) * (sheen * 0.35 + streak * 0.16);
    col += lightCol * pow (ndh, 200.0) * 0.55;

    alpha = clamp (0.10 + 0.34 * pow (facing, 3.0) + sheen * 0.30 + streak * 0.14, 0.0, 0.85);
)GLSL", "", false };

        const Material windowLight { "windowLight", R"GLSL(
    // Additive pass over the finished frame: the air in the shafts of light coming from the
    // window this rack is standing in front of. uParams = (intensity, aspect, _, _)
    vec2 sp = gl_FragCoord.xy / uViewport;
    vec2 sun = vec2 (-0.22, 1.26);
    vec2 d = sp - sun;
    d.x *= uParams.y;

    float dist = length (d);
    float ang = atan (d.y, d.x);

    // Soft shafts, matching the ones lighting the hardware
    float shaft = 0.0;
    shaft += smoothstep (0.200, 0.060, abs (ang + 1.06));
    shaft += 0.75 * smoothstep (0.140, 0.035, abs (ang + 0.80));
    shaft += 0.55 * smoothstep (0.125, 0.030, abs (ang + 1.34));
    shaft *= 0.80 + 0.20 * valueNoise (vec2 (ang * 14.0, dist * 4.0 - uTime * 0.03));
    shaft *= smoothstep (2.10, 0.30, dist);

    vec3 warm = vec3 (1.00, 0.90, 0.74);
    col = warm * shaft * 0.11;
    col += warm * exp (-dist * 2.0) * 0.07;

    alpha = uParams.x;
)GLSL", "", false };

        const Material magnifierLens { "magnifierLens", R"GLSL(
    // Screen-space loupe over a zoomed re-render of the scene. Fisheye: the centre is magnified most,
    // the edge compresses, with a little chromatic fringing like real glass.
    vec2 d = (gl_FragCoord.xy - uParams2.xy) / uParams2.z;
    float r = length (d);
    float k = uParams.x, chroma = uParams.y;
    vec2 dir = r > 1.0e-4 ? d / r : vec2 (0.0);
    float rs = r * ((1.0 - k) + k * r * r);
    vec2 uvG = 0.5 + 0.5 * dir * rs;
    vec2 uvR = 0.5 + 0.5 * dir * rs * (1.0 + chroma * r * r);
    vec2 uvB = 0.5 + 0.5 * dir * rs * (1.0 - chroma * r * r);
    col = vec3 (texture (uTex, uvR).r, texture (uTex, uvG).g, texture (uTex, uvB).b);

    // Glass: darker inner rim, bright bevel ring, specular glint at the top left, faint tint
    float rim = smoothstep (0.80, 1.0, r);
    col *= 1.0 - 0.35 * rim;
    col += vec3 (0.90, 0.92, 1.0) * smoothstep (0.93, 0.975, r) * (1.0 - smoothstep (0.975, 1.0, r)) * 0.55;
    vec2 glintDir = normalize (vec2 (-0.6, 0.8));
    float glint = pow (max (dot (dir, glintDir), 0.0), 6.0) * smoothstep (0.55, 0.9, r) * (1.0 - smoothstep (0.9, 0.99, r));
    col += vec3 (1.0) * glint * 0.35;
    col += vec3 (0.85, 0.92, 1.0) * exp (-pow ((d.x * 0.6 + d.y - 0.55) * 3.0, 2.0)) * 0.06;
    // uParams.z = glass opacity (0 = fully opaque, for callers that do not set it): the panel stays
    // faintly visible through the middle, while the rim and bevel stay solid
    float opacity = uParams.z > 0.0 ? uParams.z : 1.0;
    alpha = uParams.w * mix (opacity, 1.0, smoothstep (0.72, 0.99, r)) * (1.0 - smoothstep (0.985, 1.0, r));
)GLSL", "", false };
    }
}
