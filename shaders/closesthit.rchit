#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) rayPayloadInEXT vec4 hitValue;
hitAttributeEXT vec3 attribs;

const vec4 hello[2] = {
    vec4(0.9255, 0.8667, 0.0314, 1.0),
    vec4(0.0, 0.0, 0.0, 1.0),
};

void main() {
    // See https://github.com/SaschaWillems/Vulkan/blob/master/data/shaders/glsl/raytracingbasic/closesthit.rchit
    // TODO: Fix
    hitValue = hello[gl_PrimitiveID%2];
}
