// Simple reinhard tonemapping
vec3 reinhardSimple(in vec3 C) {
    return C / (1.0 + C);
}

// Extended reinhard tonemapping. Cwhite
// is the maximum white value of the scene, also
// known as 'white point'.
vec3 reinhardExtended(in vec3 C, in float Cwhite) {
    return (C * (1.0 + (C / vec3(Cwhite * Cwhite))))
        / (1.0f + C);
}

// ACES Tonemapping by Stephen Hill (@self_shadow)
// sRGB => XYZ => D65_2_D60 => AP1 => RRT_SAT
const mat3 ACESInputMat = mat3(
    0.59719, 0.07600, 0.02840,
    0.35458, 0.90834, 0.13383,
    0.04823, 0.01566, 0.83777);

// ODT_SAT => XYZ => D60_2_D65 => sRGB
const mat3 ACESOutputMat = mat3(
    1.60475, -0.10208, -0.00327,
    -0.53108,  1.10813, -0.07276,
    -0.07367, -0.00605,  1.07602);

vec3 RRTAndODTFit(in vec3 v) {
    vec3 a = v * (v + 0.0245786) - 0.000090537;
    vec3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
    return a / b;
}

vec3 ACESFitted(in vec3 color) {
    color = ACESInputMat * color;
    color = RRTAndODTFit(color);
    color = ACESOutputMat * color;
    return clamp(color, 0.0, 1.0);
}
