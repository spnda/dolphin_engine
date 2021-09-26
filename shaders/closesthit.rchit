#version 460
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "include/raycommon.glsl"

layout(location = 0) rayPayloadInEXT HitPayload hitPayload;
hitAttributeEXT vec3 attribs;

layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 3, set = 0) buffer Materials { Material material[]; } materials;

const vec4 hello[2] = {
    vec4(0.9255, 0.8667, 0.0314, 1.0),
    vec4(0.0, 0.0, 0.0, 1.0),
};

void main() {
    // See https://github.com/SaschaWillems/Vulkan/blob/master/data/shaders/glsl/raytracingbasic/closesthit.rchit
    // TODO: Fix
    Material material = materials.material[gl_InstanceCustomIndexEXT];
    hitPayload.hitValue = vec4(0.9255, 0.8667, 0.0314, 1.0);
}
