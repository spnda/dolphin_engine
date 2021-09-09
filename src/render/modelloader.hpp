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
        Assimp::Importer importer;

        uint32_t importFlags =
            aiProcess_FlipWindingOrder |
            aiProcess_Triangulate |
            aiProcess_PreTransformVertices |
            aiProcess_CalcTangentSpace |
            aiProcess_GenSmoothNormals;

        void loadMesh(const aiMesh* mesh, const aiMatrix4x4 transform, const aiScene* scene);
        void loadNode(const aiNode* node, const aiScene* scene);
    public:
        std::vector<Mesh> meshes = {};

        ModelLoader();

        bool loadFile(const std::string fileName);

        dp::AccelerationStructure buildAccelerationStructure(const dp::Context& context);
    };
}
