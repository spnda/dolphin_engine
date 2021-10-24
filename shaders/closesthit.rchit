#version 460
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_buffer_reference2 : require

#include "include/raycommon.glsl"

layout(location = 0) rayPayloadInEXT HitPayload hitPayload;
layout(location = 1) rayPayloadEXT bool hitPayloadOut; // Shadow payload
hitAttributeEXT vec2 attribs;

layout(buffer_reference, scalar) buffer Vertices { Vertex v[]; };
layout(buffer_reference, scalar) buffer Indices { ivec3 i[]; };

layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 3, set = 0, scalar) buffer Materials { Material m[]; } materials;
layout(binding = 4, set = 0, scalar) buffer Descriptions { ObjectDescription d[]; } descriptions;
layout(binding = 5, set = 0) uniform sampler2D textures[];

layout(push_constant) uniform PushConstants {
    vec3 lightPosition;
    float lightIntensity;
} constants;

#include "include/utilities.glsl"

vec3 computeDiffuse(Material material, vec3 normal, vec3 lightDir) {
    float dotNL = max(dot(normal, lightDir), 1.0);
    return material.diffuse.xyz * dotNL;
}

void main() {
    const vec3 barycentrics = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);

    Triangle tri = getTriangle(gl_InstanceCustomIndexEXT, gl_PrimitiveID, barycentrics);
    Material material = materials.m[tri.materialIndex];

    // Get the position and normals of the hit in world space.
    const vec3 position = tri.vert[0].position.xyz * barycentrics.x + tri.vert[1].position.xyz * barycentrics.y + tri.vert[2].position.xyz * barycentrics.z;
    const vec3 worldPos = vec3(gl_ObjectToWorldEXT * vec4(position, 1.0));
    const vec3 normal = tri.vert[0].normal.xyz * barycentrics.x + tri.vert[1].normal.xyz * barycentrics.y + tri.vert[2].normal.xyz * barycentrics.z;
    const vec3 worldNormal = normalize(vec3(normal * gl_ObjectToWorldEXT));

    // Calculate light direction and distance
    vec3 lightDirection = constants.lightPosition - worldPos;
    float lightDistance = length(lightDirection);
    lightDirection = normalize(lightDirection);

    // Get diffuse
    float intensity = 1.0;
    //float intensity = constants.lightIntensity / pow(lightDistance, 2.0);
    vec3 diffuse = computeDiffuse(material, worldNormal, lightDirection) * intensity;

    // Sample texture
    vec2 textureCoords = tri.vert[0].uv * barycentrics.x + tri.vert[1].uv * barycentrics.y + tri.vert[2].uv * barycentrics.z;
    diffuse *= texture(textures[nonuniformEXT(material.textureIndex)], textureCoords).xyz;

    // Trace a shadow ray
    if (abs(dot(worldNormal, lightDirection)) > 0) {
        vec3 shadowRayPosition = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
        hitPayloadOut = true;
        traceRayEXT(
            topLevelAS,
            gl_RayFlagsSkipClosestHitShaderEXT,
            0xFF,
            0,
            0,
            1, // shadow.rmiss
            shadowRayPosition,
            0.001,
            lightDirection,
            lightDistance,
            1 // payload (location = 1)
        );

        if (hitPayloadOut) { // Is shadowed
            diffuse *= 0.3;
        }
    }

    hitPayload.hitValue = vec4(diffuse, 1.0);
}
