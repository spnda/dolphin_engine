#pragma once

#include <vector>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "../vulkan/rt/acceleration_structure_builder.hpp"

#include "mesh.hpp"

namespace dp {
    class ModelLoader {
    private:
        const dp::Context& ctx;

        static const uint32_t importFlags =
            aiProcess_FlipWindingOrder |
            aiProcess_Triangulate |
            aiProcess_PreTransformVertices |
            aiProcess_CalcTangentSpace |
            aiProcess_GenSmoothNormals;

        Assimp::Importer importer;

        void loadMesh(const aiMesh* mesh, const aiMatrix4x4 transform, const aiScene* scene);
        void loadNode(const aiNode* node, const aiScene* scene);

        void getMatColor3(aiMaterial* material, const char* pKey, unsigned int type, unsigned int idx, glm::vec4* vec) const;
    public:
        dp::Buffer materialBuffer;

        std::vector<Material> materials = {};
        std::vector<Mesh> meshes = {};

        ModelLoader(const dp::Context& context);

        bool loadFile(const std::string fileName);

        void createMaterialBuffer();

        dp::AccelerationStructure buildAccelerationStructure(const dp::Context& context);
    };
}
