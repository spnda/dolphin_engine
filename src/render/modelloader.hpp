#pragma once

#include <vector>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "../vulkan/resource/buffer.hpp"
#include "../vulkan/resource/texture.hpp"
#include "mesh.hpp"

namespace dp {
    // fwd
    class TopLevelAccelerationStructure;

    class ModelLoader {
        const dp::Context& ctx;

        static const uint32_t importFlags =
            aiProcess_Triangulate |
            aiProcess_PreTransformVertices |
            aiProcess_CalcTangentSpace |
            aiProcess_GenSmoothNormals;

        Assimp::Importer importer;

        void loadMesh(const aiMesh* mesh, aiMatrix4x4 transform);
        void loadNode(const aiNode* node, const aiScene* scene);
        bool loadTexture(const std::string& path);
        void loadEmbeddedTexture(const aiTexture* embeddedTexture);

        static void getMatColor3(aiMaterial* material, const char* pKey, unsigned int type, unsigned int idx, glm::vec4* vec);

    public:
        dp::Buffer materialBuffer;
        dp::Buffer descriptionsBuffer;

        std::vector<Material> materials = {};
        std::vector<ObjectDescription> descriptions = {};
        std::vector<Mesh> meshes = {};
        std::vector<Texture> textures = {};

        explicit ModelLoader(const dp::Context& context);

        bool loadFile(const std::string& fileName);

        void createMaterialBuffer();
        void createDescriptionsBuffer(const dp::TopLevelAccelerationStructure& tlas);
        std::vector<VkDescriptorImageInfo> getTextureDescriptorInfos();

        dp::TopLevelAccelerationStructure buildAccelerationStructure(const dp::Context& context);
    };
}
