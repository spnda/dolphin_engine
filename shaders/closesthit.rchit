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

layout(binding = 3, set = 0, scalar) buffer Materials { Material m[]; } materials;
layout(binding = 4, set = 0, scalar) buffer Descriptions { ObjectDescription d[]; } descriptions;

vec3 computeDiffuse(Material material, vec3 normal, vec3 lightDir) {
    float dotNL = max(dot(normal, lightDir), 1.0);
    return material.diffuse.xyz * dotNL;
}

void main() {
    const vec3 lightPosition = vec3(5.0, 5.0, 0.0);
    const vec3 barycentrics = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);

    // Get the object description, materials, indices and vertices.
    ObjectDescription description = descriptions.d[gl_InstanceCustomIndexEXT];
    Material material = materials.m[description.materialIndex];
    Indices indices = Indices(description.indexBufferAddress);
    Vertices vertices = Vertices(description.vertexBufferAddress);

    // Get the three vertices for the hit triangle.
    ivec3 index = indices.i[gl_PrimitiveID];
    Vertex v0 = vertices.v[index.x];
    Vertex v1 = vertices.v[index.y];
    Vertex v2 = vertices.v[index.z];

    // Get the position and normals of the hit in world space.
    const vec3 position = v0.position.xyz * barycentrics.x + v1.position.xyz * barycentrics.y + v2.position.xyz * barycentrics.z;
    const vec3 worldPos = vec3(gl_ObjectToWorldEXT * vec4(position, 1.0));
    const vec3 normal = v0.normal.xyz * barycentrics.x + v1.normal.xyz * barycentrics.y + v2.normal.xyz * barycentrics.z;
    const vec3 worldNormal = normalize(vec3(normal * gl_ObjectToWorldEXT));

    // Calculate diffuse lighting
    vec3 diffuse = computeDiffuse(material, worldNormal, normalize(lightPosition - worldPos));
    float intensity = 32.0 / pow(length(lightPosition - worldPos), 2.0);

    hitPayload.hitValue = vec4(intensity * diffuse, 1.0);
}
