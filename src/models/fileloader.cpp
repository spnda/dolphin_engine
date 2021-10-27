#include "fileloader.hpp"

#define DDS_USE_STD_FILESYSTEM
#define STB_IMAGE_IMPLEMENTATION

#include <dds.hpp> // DirectDraw Surface
#include <fmt/core.h>
#include <stb_image.h> // PNG and more

void getMatColor3(aiMaterial* material, const char* key, unsigned int type, unsigned int idx, glm::vec4* vec) {
    aiColor4D vec4;
    aiGetMaterialColor(material, key, type, idx, &vec4);
    vec->b = vec4.b;
    vec->r = vec4.r;
    vec->g = vec4.g;
}

void dp::FileLoader::loadMesh(const aiMesh* mesh, const aiMatrix4x4 transform, const aiScene* scene) {
    if (!mesh->HasFaces()) return;

    auto meshVertices = mesh->mVertices;
    auto meshFaces = mesh->mFaces;

    dp::Mesh newMesh;
    newMesh.name = mesh->mName.data;
    newMesh.transform = {
        transform.a1, transform.a2, transform.a3, transform.a4,
        transform.b1, transform.b2, transform.b3, transform.b4,
        transform.c1, transform.c2, transform.c3, transform.c4,
    };

    for (int i = 0; i < mesh->mNumVertices; i++) {
        Vertex vertex;
        vertex.pos = glm::vec3(meshVertices[i].x, meshVertices[i].y, meshVertices[i].z);
        if (mesh->HasNormals()) {
            vertex.normals = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
        }
        vertex.uv = mesh->mTextureCoords[0] == nullptr
              ? glm::vec2(0.0)
              : glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y),
        newMesh.vertices.push_back(vertex);
    }

    for (int i = 0; i < mesh->mNumFaces; i++) {
        for (int j = 0; j < meshFaces[i].mNumIndices; j++) {
            aiFace face = meshFaces[i];
            newMesh.indices.push_back(face.mIndices[j]);
        }
    }

    newMesh.materialIndex = mesh->mMaterialIndex;

    meshes.push_back(newMesh);
}

void dp::FileLoader::loadNode(const aiNode* node, const aiScene* scene) {
    if (node->mMeshes != nullptr) {
        for (int i = 0; i < node->mNumMeshes; i++) {
            loadMesh(scene->mMeshes[node->mMeshes[i]], node->mTransformation, scene);
        }
    }

    if (node->mChildren != nullptr) {
        for (int i = 0; i < node->mNumChildren; i++) {
            loadNode(node->mChildren[i], scene);
        }
    }
}

bool dp::FileLoader::loadTexture(const std::string& path) {
    fs::path filepath = path;

    dp::TextureFile textureFile;
    textureFile.filePath = filepath;
    std::string extension = filepath.extension().string();
    // Convert extension to lower case.
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
        return std::tolower(c);
    });

    if (extension == ".dds") {
        dds::Image ddsImage;
        dds::ReadResult result = dds::readFile(filepath.string().c_str(), &ddsImage);
        if (result != dds::ReadResult::Success) {
            fmt::print(stderr, "Failed to read DDS file: {}", result);
            return false;
        }
        textureFile.width = ddsImage.width;
        textureFile.height = ddsImage.height;
        textureFile.format = dds::getVulkanFormat(ddsImage.format, ddsImage.supportsAlpha);
        textureFile.mipLevels = ddsImage.numMips;
        textureFile.pixels.assign(ddsImage.data.begin(), ddsImage.data.end());
    } else if (extension == ".png" || extension == ".jpg" || extension == ".bmp") {
        // STB supports JPG, PNG, TGA, BMP, PSD, GIF, HDR, PIC.
        int tWidth, tHeight, channels; // Channels should always be 4 because we ask STB for RGBA.
        stbi_uc* stbPixels = stbi_load(filepath.string().c_str(), &tWidth, &tHeight, &channels, STBI_rgb_alpha);
        if (!stbPixels) {
            fmt::print(stderr, "Failed to load texture file {}\n", path);
            return false;
        }
        textureFile.width = tWidth;
        textureFile.height = tHeight;
        textureFile.format = VK_FORMAT_R8G8B8A8_SRGB; // The format STB uses.
        textureFile.pixels.resize(tWidth * tHeight * 4);

        textureFile.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(tWidth, tHeight))));

        memcpy(textureFile.pixels.data(), stbPixels, textureFile.pixels.size());

        stbi_image_free(stbPixels);
    } else {
        return false;
    }

    textures.push_back(textureFile);
    return true;
}


dp::FileLoader::FileLoader(const FileLoader& loader) :
        meshes(loader.meshes),
        materials(loader.materials),
        textures(loader.textures) {
}

dp::FileLoader& dp::FileLoader::operator=(const dp::FileLoader& fileLoader) {
    meshes.assign(fileLoader.meshes.begin(), fileLoader.meshes.end());
    materials.assign(fileLoader.materials.begin(), fileLoader.materials.end());
    textures.assign(fileLoader.textures.begin(), fileLoader.textures.end());
    return *this;
}

bool dp::FileLoader::loadFile(const fs::path& fileName) {
    meshes.clear();
    materials.clear();
    textures.clear();

    const aiScene* scene = importer.ReadFile(fileName.string(), importFlags);

    if (!scene || scene->mRootNode == nullptr) {
        fmt::print("{}\n", importer.GetErrorString());
        return false;
    }

    // Load Meshes
    loadNode(scene->mRootNode, scene);

    // Load Materials
    if (scene->HasMaterials()) {
        for (uint32_t i = 0; i < scene->mNumMaterials; i++) {
            dp::Material material;
            aiMaterial* mat = scene->mMaterials[i];
            getMatColor3(mat, AI_MATKEY_COLOR_DIFFUSE, &material.diffuse);
            getMatColor3(mat, AI_MATKEY_COLOR_SPECULAR, &material.specular);
            getMatColor3(mat, AI_MATKEY_COLOR_EMISSIVE, &material.emissive);

            // Load each diffuse texture. For now, we only support a single texture here.
            for (uint32_t j = 0; j < 1 /* mat->GetTextureCount(aiTextureType_DIFFUSE) */; j++) {
                aiString texturePath;
                mat->GetTexture(aiTextureType_DIFFUSE, j, &texturePath);

                const aiTexture* embeddedTexture = scene->GetEmbeddedTexture(texturePath.C_Str());
                if (embeddedTexture != nullptr) {
                    // TODO
                } else if (texturePath.length != 0) {
                    // Take the path of the model as a relative path.
                    // TODO: Check for duplicate textures somehow?
                    fs::path parentFolder = fs::path(fileName).parent_path();
                    bool loaded = loadTexture(parentFolder.string() + "\\" + texturePath.C_Str());
                    if (loaded) {
                        material.textureIndex = static_cast<int32_t>(textures.size() - 1);
                    }
                }
            }
            materials.push_back(material);
        }
    }

    fmt::print("Finished loading file!\n");
    return true;
}
