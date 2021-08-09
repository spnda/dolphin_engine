#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) rayPayloadInEXT vec3 hitValue;

void main() {
    // See https://github.com/SaschaWillems/Vulkan/blob/master/data/shaders/glsl/raytracingbasic/closesthit.rchit
    // TODO: Fix
    hitValue = vec3(0.0);
}
