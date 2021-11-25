#pragma once

#include <filesystem>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <tiny_gltf.h>

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
            aiProcess_ValidateDataStructure |
            aiProcess_GenNormals;

        // ASSIMP
        [[nodiscard]] bool loadAssimpFile(const fs::path& fileName);
        void loadAssimpMesh(const aiMesh* mesh, aiMatrix4x4 transform, const aiScene* scene);
        void loadAssimpNode(const aiNode* node, const aiScene* scene);
        /** Loads a texture into local memory. Returns 0 if failed, the texture index otherwise. */
        [[nodiscard]] int32_t loadAssimpTexture(const std::string& path);
        [[nodiscard]] int32_t loadEmbeddedAssimpTexture(const aiTexture* texture);

        // TINYGLTF
        bool loadGltfFile(const fs::path& fileName);
        void loadGlftMesh(tinygltf::Model& model, const tinygltf::Mesh& mesh, const tinygltf::Node& node);
        void loadGltfNode(tinygltf::Model& model, const tinygltf::Node& node);

    public:
        std::vector<dp::Mesh> meshes;
        std::vector<dp::Material> materials;
        std::vector<dp::TextureFile> textures;

        explicit FileLoader() = default;
        FileLoader(const FileLoader&) = default;
        FileLoader& operator=(const dp::FileLoader& fileLoader);

        bool loadFile(const fs::path& fileName);
    };
}
