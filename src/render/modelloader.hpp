#pragma once

#include <vector>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "../render/vertex.hpp"

namespace dp {
    class ModelLoader {
    private:
        Assimp::Importer importer;

        uint32_t importFlags =
            aiProcess_FlipWindingOrder |
            aiProcess_Triangulate |
            aiProcess_PreTransformVertices |
            aiProcess_CalcTangentSpace | aiProcess_GenSmoothNormals;
            // aiProcess_JoinIdenticalVertices; // Reduce memory footprint and use indices.

        void loadMesh(const aiMesh* mesh, const aiScene* scene);
        void loadNode(const aiNode* node, const aiScene* scene);
    public:
        std::vector<Vertex> vertices = {};
        std::vector<Index> indices = {};

        ModelLoader();

        bool loadFile(const std::string fileName);
    };
}
