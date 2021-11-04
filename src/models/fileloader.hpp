#pragma once

#include <filesystem>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "mesh.hpp"
#include "../vulkan/resource/texture.hpp"

namespace fs = std::filesystem;

namespace dp {
    /** A model loader dedicated to a single file (OBJ, FBX, GLTF, ...) */
    class FileLoader {
        static const uint32_t importFlags =
            aiProcess_FlipWindingOrder |
            aiProcess_JoinIdenticalVertices |
            aiProcess_Triangulate |
            aiProcess_SortByPType |
            aiProcess_GenNormals;

        Assimp::Importer importer;

        void loadMesh(const aiMesh* mesh, aiMatrix4x4 transform, const aiScene* scene);
        void loadNode(const aiNode* node, const aiScene* scene);
        /** Loads a texture into local memory. Returns 0 if failed, the texture index otherwise. */
        [[nodiscard]] int32_t loadTexture(const std::string& path);

    public:
        std::vector<dp::Mesh> meshes;
        std::vector<dp::Material> materials;
        std::vector<dp::TextureFile> textures;

        explicit FileLoader() = default;
        FileLoader(const FileLoader&);
        FileLoader& operator=(const dp::FileLoader& fileLoader);

        bool loadFile(const fs::path& fileName);
    };
}
