#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_debug_printf : enable

layout(location = 1) rayPayloadInEXT bool isShadowed;

// Simple miss shader for detecting shadows.
// If it misses any geometry and finds the light (it missed everything)
// it is considered not shadowed, which is why we then set it to false.
void main() {
    isShadowed = false;
}
