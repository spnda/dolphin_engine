#extension GL_ARB_gpu_shader_int64 : enable

struct HitPayload {
    vec4 hitValue;
    vec3 origin;
};

struct Vertex {
    vec4 position;
    vec4 normal;
};

struct ObjectDescription {
    uint64_t vertexBufferAddress;
    uint64_t indexBufferAddress;
    int materialIndex;
};

struct Material {
    vec4 diffuse;
    vec4 specular;
    vec4 emissive;
};
