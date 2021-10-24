#extension GL_ARB_gpu_shader_int64 : enable

struct HitPayload {
    vec4 hitValue;
    vec3 origin;
};

struct Vertex {
    vec3 position;
    vec3 normal;
    vec2 uv;
    float padding;
};

/** A single mesh/BLAS. */
struct ObjectDescription {
    uint64_t vertexBufferAddress;
    uint64_t indexBufferAddress;
    uint materialIndex;
};

/** Represents a material with different base colours and
 *  associated texture indices */
struct Material {
    vec4 diffuse;
    vec4 specular;
    vec4 emissive;
    uint textureIndex;
};

/** Represents three vertices and their properties. */
struct Triangle {
    Vertex vert[3];
    uint materialIndex;
};
