#version 460
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_ray_tracing : enable

#include "include/raycommon.glsl"

layout(location = 0) rayPayloadInEXT HitPayload hitPayload;

void main() {
    // Give the sky a bit of a gradient
    vec3 direction = gl_WorldRayDirectionEXT;
    float t = 0.5 * (direction.y + 1.0);
    hitPayload.hitValue = (1.0 - t) * vec3(1.0, 1.0, 1.0) + t * vec3(0.5, 0.7, 1.0);
    hitPayload.hitValue *= 2; // Give the sky a bit of energy.
}
