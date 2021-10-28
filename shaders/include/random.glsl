const float PHI = 1.61803398874989484820459; // Φ = Golden Ratio 

// Gold noise function, see https://www.shadertoy.com/view/ltB3zD
float randomNoise(in vec2 xy, in float seed) {
    return fract(tan(distance(xy * PHI, xy) * seed) * xy.x);
}

vec3 randVec3(in vec2 co, in float seed) {
    // We offset the coordinates by just arbitrary numbers so we don't get the same float
    // for all three components of the vector.
    return vec3(randomNoise(co, seed + 0), randomNoise(co, seed + 1), randomNoise(co, seed + 2));
}

// Takes given vector with values in range of [iMin, iMax] and maps it to [oMin, oMax].
vec3 clampRange(in vec3 v, in float iMin, in float iMax, in float oMin, in float oMax) {
    return ((v - iMin) * (oMax - oMin) / (iMax - iMin)) + oMin;
}
