#extension GL_ARB_gpu_shader_int64 : enable

struct HitPayload {
    vec3 hitValue;
    vec3 origin;
    vec3 rayDirection;
    uint rayRecursionDepth;
};

struct Vertex {
    vec3 position;
    vec3 normal;
    vec2 uv;
    float padding;
};

/** A single BLAS. */
struct InstanceDescription {
    uint64_t vertexBufferAddress;
    uint64_t indexBufferAddress;
    uint64_t geometryBufferAddress;
};

/** A single geometry inside a BLAS */
struct GeometryDescription {
    uint64_t vertexOffset;
    uint64_t indexOffset;
    uint materialIndex;
};

/** Represents a material with different base colours and
 *  associated texture indices */
struct Material {
    vec3 baseColor;
    float metallicFactor;
    float roughnessFactor;
    int baseTextureIndex;
    int normalTextureIndex;
    int pbrTextureIndex;
};

/** Represents three vertices and their properties. */
struct Triangle {
    Vertex vert[3];
    uint materialIndex;
};
