// PlateVerb surface and plate materials (no JUCE dependency).
//
// Room / chamber / hall / IR surfaces: random-incidence absorption coefficients per octave
// (125 Hz - 4 kHz) from the standard tables (sengpielaudio RT60 coefficient table). The decay per band
// follows Sabine (RT ~ 1 / (mean absorption + air absorption)), with air absorption from ISO 9613-1
// at 20 C / 50 % RH, scaled by the space's volume-to-surface ratio. Every material is applied relative to
// smooth plaster, which is what the built-in algorithms (and real echo chambers) already sound like,
// so "Plaster" = the sound you had before. Scattering values are estimates (smooth surfaces reflect
// specularly, rough ones scatter) and only nudge the diffusion.
//
// Plates: bending-wave physics of a thin plate. Wave speed ~ (E/rho)^(1/4) (slower = softer attack),
// modal density ~ sqrt(rho/E) (higher = denser, smoother, more chamber-like), and material loss factor
// eta adds high-frequency damping (RT60 = 2.2 / (f * eta)). E and rho from the IRCAM Modalys material
// table; loss factors chosen inside the ranges given by COMSOL's damping reference (steel/aluminium
// 1e-4.., copper 1e-3..); brass and gold are estimates. Steel (EMT 140) = the sound you had before.
#pragma once
#include <cmath>
#include <algorithm>

namespace pv {

struct SurfaceMaterial { const char* name; float a[6]; float scatter; };
constexpr int kNumSurfaces = 13;
constexpr int kDefaultSurface = 0;          // plaster = unchanged sound
inline const SurfaceMaterial& surfaceMaterial (int i) {
    static const SurfaceMaterial m[kNumSurfaces] = {
        { "Plaster (smooth)",       { 0.013f, 0.015f, 0.02f, 0.03f, 0.04f, 0.05f }, 0.10f },
        { "Wood panels",            { 0.28f, 0.22f, 0.17f, 0.09f, 0.10f, 0.11f },   0.15f },
        { "Wood floor / thick wood",{ 0.15f, 0.11f, 0.10f, 0.07f, 0.06f, 0.07f },   0.12f },
        { "Stone / marble",         { 0.01f, 0.01f, 0.01f, 0.01f, 0.02f, 0.02f },   0.05f },
        { "Tiles",                  { 0.01f, 0.01f, 0.015f, 0.02f, 0.02f, 0.02f },  0.05f },
        { "Brick",                  { 0.03f, 0.03f, 0.03f, 0.04f, 0.05f, 0.07f },   0.25f },
        { "Concrete (painted)",     { 0.10f, 0.05f, 0.06f, 0.07f, 0.09f, 0.08f },   0.12f },
        { "Rough concrete block",   { 0.36f, 0.44f, 0.31f, 0.29f, 0.39f, 0.25f },   0.50f },
        { "Glass",                  { 0.35f, 0.25f, 0.18f, 0.12f, 0.07f, 0.04f },   0.05f },
        { "Heavy curtains",         { 0.14f, 0.35f, 0.55f, 0.72f, 0.70f, 0.65f },   0.30f },
        { "Carpet",                 { 0.02f, 0.06f, 0.14f, 0.37f, 0.60f, 0.65f },   0.20f },
        { "Water / ice",            { 0.008f, 0.008f, 0.013f, 0.015f, 0.020f, 0.025f }, 0.02f },
        { "Audience (seated)",      { 0.60f, 0.74f, 0.88f, 0.96f, 0.93f, 0.85f },   0.60f },
    };
    return m[std::min (std::max (i, 0), kNumSurfaces - 1)];
}

struct PlateMaterial { const char* name; float E, rho, eta; };
constexpr int kNumPlates = 6;
constexpr int kDefaultPlate = 0;            // steel = unchanged sound
inline const PlateMaterial& plateMaterial (int i) {
    static const PlateMaterial m[kNumPlates] = {
        { "Steel (EMT 140-style)",     2.00e11f, 7700.0f,  1.0e-4f },
        { "Stainless steel",           1.93e11f, 8000.0f,  0.7e-4f },
        { "Aluminium",                 7.00e10f, 2700.0f,  1.0e-4f },
        { "Brass",                     1.12e11f, 8500.0f,  2.5e-4f },
        { "Copper",                    1.19e11f, 8940.0f,  4.0e-4f },
        { "Gold foil (EMT 240-style)", 7.90e10f, 19300.0f, 6.0e-4f },
    };
    return m[std::min (std::max (i, 0), kNumPlates - 1)];
}

// What a (blended) material does to an algorithm. Identity for plaster / steel.
struct MaterialEffect {
    float lowMul = 1, midMul = 1, highMul = 1;   // decay multipliers per band (on top of the knobs)
    float extraLoss[3] = { 0, 0, 0 };             // plates: added 1/RT per band (low, mid, high)
    float diffOffset = 0;                        // added to diffusion
    float sizeMul = 1, attackMul = 1;            // plates: tank size (density), onset length (wave speed)
    float erLowDb = 0, erHighDb = 0;             // reflections' tone relative to plaster
};

// surfaces: volume-to-surface ratio V/S in metres decides how much air absorption matters
inline MaterialEffect surfaceEffect (int a, int b, float blend, float vOverS) {
    const auto& A = surfaceMaterial (a); const auto& B = surfaceMaterial (b);
    const auto& R = surfaceMaterial (kDefaultSurface);
    const float t = std::min (1.0f, std::max (0.0f, blend));
    // air absorption, ISO 9613-1, 20 C / 50 % RH, dB/km at 125..4k and 8k
    const float airDb[7] = { 0.4f, 1.0f, 1.9f, 3.7f, 9.7f, 32.8f, 117.0f };
    float air[7]; for (int k = 0; k < 7; ++k) air[k] = 4.0f * (airDb[k] / 4343.0f) * vOverS;   // as absorption coefficient
    auto mix = [&] (int k) { return A.a[k] + (B.a[k] - A.a[k]) * t; };
    auto bands = [&] (auto alpha, float out[3]) {
        const float lo = 0.5f * (alpha (0) + alpha (1)) + 0.5f * (air[0] + air[1]);
        const float mid = (alpha (2) + alpha (3) + alpha (4)) / 3.0f + (air[2] + air[3] + air[4]) / 3.0f;
        const float hi = alpha (5) + 0.5f * (air[5] + air[6]);
        out[0] = 1.0f / lo; out[1] = 1.0f / mid; out[2] = 1.0f / hi;        // Sabine: RT ~ 1 / absorption
    };
    float m[3], r[3];
    bands ([&] (int k) { return mix (k); }, m);
    bands ([&] (int k) { return R.a[k]; }, r);
    MaterialEffect e;
    e.lowMul = std::min (3.0f, std::max (0.3f, (m[0] / m[1]) / (r[0] / r[1])));
    e.highMul = std::min (2.0f, std::max (0.25f, (m[2] / m[1]) / (r[2] / r[1])));
    e.midMul = 1.0f;
    const float sc = A.scatter + (B.scatter - A.scatter) * t;
    e.diffOffset = std::min (0.35f, std::max (-0.25f, 0.6f * (sc - R.scatter)));
    // each reflection keeps sqrt(1 - alpha) of its pressure; ~3 bounces before the late tail
    auto refl = [&] (float x) { return 10.0f * std::log10 (std::max (0.02f, 1.0f - x)); };
    const float lo = refl (0.5f * (mix (0) + mix (1))) - refl (0.5f * (R.a[0] + R.a[1]));
    const float md = refl ((mix (2) + mix (3) + mix (4)) / 3.0f) - refl ((R.a[2] + R.a[3] + R.a[4]) / 3.0f);
    const float hi = refl (mix (5)) - refl (R.a[5]);
    e.erLowDb = 3.0f * (lo - md); e.erHighDb = 3.0f * (hi - md);
    return e;
}

inline MaterialEffect plateEffect (int a, int b, float blend) {
    const auto& A = plateMaterial (a); const auto& B = plateMaterial (b); const auto& R = plateMaterial (kDefaultPlate);
    const float t = std::min (1.0f, std::max (0.0f, blend));
    const float E = A.E + (B.E - A.E) * t, rho = A.rho + (B.rho - A.rho) * t, eta = A.eta + (B.eta - A.eta) * t;
    const float speed = std::pow ((E / rho) / (R.E / R.rho), 0.25f);          // bending-wave speed vs steel
    const float density = std::sqrt ((rho / E) / (R.rho / R.E));              // modal density vs steel
    MaterialEffect e;
    e.sizeMul = std::pow (density, 0.25f);
    e.diffOffset = std::min (0.35f, std::max (-0.2f, 0.25f * std::log2 (density)));
    e.attackMul = 1.0f / speed;
    const float f[3] = { 150.0f, 1000.0f, 6000.0f };
    for (int k = 0; k < 3; ++k) e.extraLoss[k] = (eta - R.eta) * f[k] / 2.2f;   // 1/RT added per band
    return e;
}

} // namespace pv
