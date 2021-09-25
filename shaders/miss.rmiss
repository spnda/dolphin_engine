#version 460
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_ray_tracing : enable

#include "include/raycommon.glsl"

layout(location = 0) rayPayloadInEXT HitPayload hitPayload;

void main() {
    hitPayload.hitValue = vec4(0.0, 0.0, 0.0, 1.0);
}
