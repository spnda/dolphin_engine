#version 460
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_buffer_reference2 : require

#include "include/raycommon.glsl"

layout(location = 0) rayPayloadInEXT HitPayload hitPayload;
hitAttributeEXT vec2 attribs;

layout(buffer_reference, scalar) buffer Vertices { Vertex v[]; };
layout(buffer_reference, scalar) buffer Indices { ivec3 i[]; };

layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 3, set = 0, scalar) buffer Materials { Material m[]; } materials;
layout(binding = 4, set = 0, scalar) buffer Descriptions { ObjectDescription d[]; } descriptions;
layout(binding = 5, set = 0) uniform sampler2D textures[];

layout(push_constant) uniform PushConstants {
    // The current system time in seconds. Used for RNG seeds.
    float iTime;
} constants;

#include "include/rayutilities.glsl"
#include "include/random.glsl"

vec3 getBounceRayDirection(in vec3 normal) {
    // Random direction based on the time and our work group and the ray recursion depth to get pretty random seeds.
    vec3 randDir = clampRange(randVec3(gl_LaunchIDEXT.xy + hitPayload.rayRecursionDepth, fract(constants.iTime)), 0.0, 1.0, -1.0, 1.0);
    return normalize(normal + randDir);
}

void main() {
    const vec3 barycentrics = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);

    Triangle tri = getTriangle(gl_InstanceCustomIndexEXT, gl_PrimitiveID, barycentrics);
    Material material = materials.m[tri.materialIndex];

    // Get the position and normals of the hit in world space.
    const vec3 position = tri.vert[0].position * barycentrics.x + tri.vert[1].position * barycentrics.y + tri.vert[2].position * barycentrics.z;
    const vec3 worldPos = vec3(gl_ObjectToWorldEXT * vec4(position, 1.0));
    const vec3 normal = tri.vert[0].normal * barycentrics.x + tri.vert[1].normal * barycentrics.y + tri.vert[2].normal * barycentrics.z;
    const vec3 worldNormal = normalize(vec3(normal * gl_ObjectToWorldEXT));

    // Sample texture
    vec4 sampleColor;
    if (material.textureIndex != 0) {
        vec2 textureCoords = tri.vert[0].uv * barycentrics.x + tri.vert[1].uv * barycentrics.y + tri.vert[2].uv * barycentrics.z;
        sampleColor = texture(textures[nonuniformEXT(material.textureIndex)], textureCoords);
    } else {
        sampleColor = material.diffuse;
    }

    // We don't want to exceed the maximum ray recursion depth of 5.
    if (hitPayload.rayRecursionDepth >= 2) {
        hitPayload.hitValue = vec4(0.0);
        return;
    }

    // Bounce a random diffuse ray.
    hitPayload.origin = worldPos;
    hitPayload.rayDirection = getBounceRayDirection(normal);
    float tmin = 0.001;
    float tmax = 10000.0;
    hitPayload.rayRecursionDepth++;
    traceRayEXT(topLevelAS, gl_RayFlagsNoneEXT, 0xFF, 0, 0, 0, hitPayload.origin, tmin, hitPayload.rayDirection, tmax, 0);
    hitPayload.hitValue = sampleColor * (0.5 * hitPayload.hitValue);
}
