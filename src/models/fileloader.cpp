#include "fileloader.hpp"

#define DDS_USE_STD_FILESYSTEM
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <dds.hpp> // DirectDraw Surface
#include <fmt/core.h>
#include <glm/gtc/type_ptr.hpp> // glm::make_vec3
#include <tiny_gltf.h> // Already includes stb_image.h

void getMatColor3(aiMaterial* material, const char* key, unsigned int type, unsigned int idx, glm::vec3* vec) {
    aiColor4D vec4;
    aiGetMaterialColor(material, key, type, idx, &vec4);
    vec->b = vec4.b;
    vec->r = vec4.r;
    vec->g = vec4.g;
}

void dp::FileLoader::loadAssimpMesh(const aiMesh* mesh, aiMatrix4x4 transform, const aiScene* scene) {
    if (!mesh->HasFaces()) return;

    auto meshVertices = mesh->mVertices;
    auto meshFaces = mesh->mFaces;

    dp::Mesh newMesh = {};
    newMesh.name = mesh->mName.data;
    newMesh.transform = {
        transform.a1, transform.a2, transform.a3, transform.a4,
        transform.b1, transform.b2, transform.b3, transform.b4,
        transform.c1, transform.c2, transform.c3, transform.c4,
    };

    dp::Primitive newPrimitive = {};
    newPrimitive.materialIndex = mesh->mMaterialIndex;

    for (int i = 0; i < mesh->mNumVertices; i++) {
        dp::Vertex vertex;
        vertex.pos = glm::vec3(meshVertices[i].x, meshVertices[i].y, meshVertices[i].z);
        if (mesh->HasNormals()) {
            vertex.normals = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
        }
        vertex.uv = mesh->mTextureCoords[0] == nullptr
                    ? glm::vec2(0.0)
                    : glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y),
            newPrimitive.vertices.push_back(vertex);
    }

    for (int i = 0; i < mesh->mNumFaces; i++) {
        for (int j = 0; j < meshFaces[i].mNumIndices; j++) {
            aiFace face = meshFaces[i];
            newPrimitive.indices.push_back(face.mIndices[j]);
        }
    }

    newMesh.primitives.push_back(newPrimitive);
    meshes.push_back(newMesh);
}

void dp::FileLoader::loadAssimpNode(const aiNode* node, const aiScene* scene) {
    if (node->mMeshes != nullptr) {
        for (int i = 0; i < node->mNumMeshes; i++) {
            loadAssimpMesh(scene->mMeshes[node->mMeshes[i]], node->mTransformation, scene);
        }
    }

    if (node->mChildren != nullptr) {
        for (int i = 0; i < node->mNumChildren; i++) {
            loadAssimpNode(node->mChildren[i], scene);
        }
    }
}

int32_t dp::FileLoader::loadAssimpTexture(const std::string& path) {
    fs::path filepath = path;
    // Check if a texture with the same filepath already exists
    for (auto tex : textures) {
        if (tex.filePath == filepath) {
            return static_cast<int32_t>(&tex - &textures[0]);
        }
    }

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
    return static_cast<int32_t>(textures.size() - 1);
}

int32_t dp::FileLoader::loadEmbeddedAssimpTexture(const aiTexture* texture) {
    // Check if a texture with the same filepath already exists
    std::string textureFileName = texture->mFilename.C_Str();
    if (!textureFileName.empty()) { // Embedded textures might not be named in some formats
        for (auto tex: textures) {
            if (strcmp(reinterpret_cast<const char*>(tex.filePath.c_str()), textureFileName.c_str()) == 0) {
                return static_cast<int32_t>(&tex - &textures[0]);
            }
        }
    }

    int tWidth, tHeight, channels;
    stbi_uc* pixels;
    if (texture->mHeight == 0) {
        pixels = stbi_load_from_memory(reinterpret_cast<unsigned char*>(texture->pcData), static_cast<int>(texture->mWidth), &tWidth, &tHeight, &channels, STBI_rgb_alpha);
    } else {
        pixels = stbi_load_from_memory(reinterpret_cast<unsigned char*>(texture->pcData), static_cast<int>(texture->mWidth * texture->mHeight), &tWidth, &tHeight, &channels, STBI_rgb_alpha);
    }
    if (!pixels) {
        fmt::print(stderr, "Failed to load texture file {}\n", texture->mFilename.C_Str());
        return false;
    }

    dp::TextureFile textureFile;
    textureFile.filePath = texture->mFilename.C_Str();
    textureFile.width = tWidth;
    textureFile.height = tHeight;
    textureFile.format = VK_FORMAT_R8G8B8A8_SRGB;

    textureFile.pixels.resize(tWidth * tHeight * 4);
    memcpy(textureFile.pixels.data(), pixels, textureFile.pixels.size());

    textures.push_back(textureFile);
    return static_cast<int32_t>(textures.size() - 1);
}

bool dp::FileLoader::loadAssimpFile(const fs::path& fileName) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(fileName.string(), importFlags);
    if (!scene || scene->mRootNode == nullptr) {
        fmt::print("{}\n", importer.GetErrorString());
        return false;
    }

    // Load Meshes
    loadAssimpNode(scene->mRootNode, scene);

    // Load Materials
    if (scene->HasMaterials()) {
        for (uint32_t i = 0; i < scene->mNumMaterials; i++) {
            dp::Material material;
            aiMaterial* mat = scene->mMaterials[i];
            /* getMatColor3(mat, AI_MATKEY_COLOR_DIFFUSE, &material.diffuse);
            getMatColor3(mat, AI_MATKEY_COLOR_SPECULAR, &material.specular);
            getMatColor3(mat, AI_MATKEY_COLOR_EMISSIVE, &material.emissive); */

            // Load each diffuse texture. For now, we only support a single texture here.
            for (uint32_t j = 0; j < 1 /* mat->GetTextureCount(aiTextureType_DIFFUSE) */; j++) {
                aiString texturePath;
                mat->GetTexture(aiTextureType_DIFFUSE, j, &texturePath);

                const aiTexture* embeddedTexture = scene->GetEmbeddedTexture(texturePath.C_Str());
                if (embeddedTexture != nullptr) {
                    auto textureIndex = loadEmbeddedAssimpTexture(embeddedTexture);
                    if (textureIndex) {
                        material.baseTextureIndex = textureIndex;
                    }
                } else if (texturePath.length != 0) {
                    // Take the path of the model as a relative path.
                    fs::path parentFolder = fs::path(fileName).parent_path();
                    int32_t textureIndex = loadAssimpTexture(parentFolder.string() + "\\" + texturePath.C_Str());
                    if (textureIndex) {
                        material.baseTextureIndex = textureIndex;
                    }
                }
            }
            materials.push_back(material);
        }
    }

    return true;
}


void dp::FileLoader::loadGlftMesh(tinygltf::Model& model, const tinygltf::Mesh& mesh, const tinygltf::Node& node) {
    dp::Mesh newMesh;
    newMesh.name = mesh.name;

    auto translation = glm::vec3(0.0f);
    if (node.translation.size() == 3) {
        translation = glm::make_vec3(node.translation.data());
    }
    auto rotation = glm::mat4(1.0f);
    if (node.rotation.size() == 4) {
        glm::quat q = glm::make_quat(node.rotation.data());
    }
    auto scale = glm::vec3(1.0f);
    if (node.scale.size() == 3) {
        scale = glm::make_vec3(node.scale.data());
    }
    auto matrix = glm::mat4(1.0f);
    if (node.matrix.size() == 16) {
        matrix = glm::make_mat4x4(node.matrix.data());
    }

    glm::mat4 newMatrix = glm::translate(glm::mat4(1.0f), translation) * glm::mat4(rotation) * glm::scale(glm::mat4(1.0f), scale) * matrix;
    newMesh.transform = {
        newMatrix[0][0], newMatrix[1][0], newMatrix[2][0], newMatrix[3][0],
        newMatrix[0][1], newMatrix[1][1], newMatrix[2][1], newMatrix[3][1],
        newMatrix[0][2], newMatrix[1][2], newMatrix[2][2], newMatrix[3][2],
    };

    // Simple helper for writing a pointer of data into a vector. We specifically
    // don't memcpy the data as the source data might be using a different type,
    // e.g. short.
    auto writeToVector = [&]<typename T, typename S>(const T* src, const uint32_t count, std::vector<S>& out) {
        out.resize(count);
        for (size_t i = 0; i < count; ++i)
            out[i] = static_cast<T>(src[i]);
    };

    auto getTypeSizeInBytes = [](uint32_t type) -> int32_t {
        return tinygltf::GetNumComponentsInType(type) * tinygltf::GetComponentSizeInBytes(TINYGLTF_COMPONENT_TYPE_FLOAT);
    };

    for (const auto& primitive : mesh.primitives) {
        dp::Primitive newPrimitive = {};
        newPrimitive.materialIndex = primitive.material;

        // Load primitive attributes (pos, normals, uv, ...)
        {
            // We require a position attribute.
            if (primitive.attributes.find("POSITION") == primitive.attributes.end())
                continue;

            // We will first load all the different buffers (possibly the same buffer at a different
            // offset) and store them as pointers.
            struct PrimitiveBufferValue {
                const float* data = nullptr;
                uint64_t stride = 0;
            };

            // Vertex positions
            uint32_t vertexCount;
            PrimitiveBufferValue positions = {};
            {
                const auto& posAccessor = model.accessors[primitive.attributes.find("POSITION")->second];
                const auto& posBufferView = model.bufferViews[posAccessor.bufferView];
                positions.data = reinterpret_cast<const float*>(&(model.buffers[posBufferView.buffer].data[posAccessor.byteOffset + posBufferView.byteOffset]));
                positions.stride = posAccessor.ByteStride(posBufferView)
                    ? (posAccessor.ByteStride(posBufferView) / sizeof(float))
                    : getTypeSizeInBytes(TINYGLTF_TYPE_VEC3);
                vertexCount = posAccessor.count;
            }

            // Normals
            PrimitiveBufferValue normals = {};
            if (primitive.attributes.find("NORMAL") != primitive.attributes.end()) {
                const auto& normalsAccessor = model.accessors[primitive.attributes.find("NORMAL")->second];
                const auto& normalsBufferView = model.bufferViews[normalsAccessor.bufferView];
                normals.data = reinterpret_cast<const float*>(&(model.buffers[normalsBufferView.buffer].data[normalsAccessor.byteOffset + normalsBufferView.byteOffset]));
                normals.stride = normalsAccessor.ByteStride(normalsBufferView)
                    ? (normalsAccessor.ByteStride(normalsBufferView) / sizeof(float))
                    : getTypeSizeInBytes(TINYGLTF_TYPE_VEC3);
            }

            // Texture coordinates
            PrimitiveBufferValue uvs = {};
            if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end()) {
                const auto& uvsAccessor = model.accessors[primitive.attributes.find("TEXCOORD_0")->second];
                const auto& uvsBufferView = model.bufferViews[uvsAccessor.bufferView];
                uvs.data = reinterpret_cast<const float*>(&(model.buffers[uvsBufferView.buffer].data[uvsAccessor.byteOffset + uvsBufferView.byteOffset]));
                uvs.stride = uvsAccessor.ByteStride(uvsBufferView)
                    ? (uvsAccessor.ByteStride(uvsBufferView) / sizeof(float))
                    : getTypeSizeInBytes(TINYGLTF_TYPE_VEC2);
            }

            // Get the data for each Vertex and store the data as a new Vertex.
            for (size_t i = 0; i < vertexCount; ++i) {
                dp::Vertex newVertex = {};
                newVertex.pos = glm::make_vec3(&positions.data[i * positions.stride]);
                if (normals.data != nullptr)
                    newVertex.normals = glm::make_vec3(&normals.data[i * normals.stride]);
                if (uvs.data != nullptr)
                    newVertex.uv = glm::make_vec2(&uvs.data[i * uvs.stride]);

                newPrimitive.vertices.push_back(newVertex);
            }
        }

        // Indexed geometry is not a must and is not handled as an attribute for some reason...
        if (primitive.indices >= 0) {
            const auto& accessor = model.accessors[primitive.indices];
            const auto& bufferView = model.bufferViews[accessor.bufferView];
            const auto& buffer = model.buffers[bufferView.buffer];

            auto indexCount = static_cast<uint32_t>(accessor.count);
            newPrimitive.indices.reserve(indexCount);

            const void* dataPtr = &(buffer.data[accessor.byteOffset + bufferView.byteOffset]);
            // The indices could be in a different format. In the future, we should convert these automatically to be safe.
            switch (accessor.componentType) {
                case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
                    const auto* buf = static_cast<const dp::Index*>(dataPtr);
                    writeToVector(buf, indexCount, newPrimitive.indices);
                    break;
                }
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
                    const auto* buf = static_cast<const uint16_t*>(dataPtr);
                    writeToVector(buf, indexCount, newPrimitive.indices);
                    break;
                }
                default: {
                    fmt::print(stderr, "Index component type {} unsupported", accessor.componentType);
                    break;
                }
            }
        } else {
            // Generate indices?
            newPrimitive.indexType = VK_INDEX_TYPE_NONE_KHR;
        }

        newMesh.primitives.push_back(newPrimitive);
    }

    meshes.push_back(newMesh);
}

void dp::FileLoader::loadGltfNode(tinygltf::Model& model, const tinygltf::Node& node) {
    // TODO: Add support for node matrices (rotation, translation, scale).
    if (node.mesh > -1) {
        loadGlftMesh(model, model.meshes[node.mesh], node);
    }

    for (auto i : node.children) {
        loadGltfNode(model, model.nodes[i]);
    }
}

bool dp::FileLoader::loadGltfFile(const fs::path& fileName) {
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    fs::path ext = fileName.extension();
    bool success = false;
    if (ext.compare(".glb") == 0) {
        success = loader.LoadBinaryFromFile(&model, &err, &warn, fileName.string());
    } else if (ext.compare(".gltf") == 0) {
        success = loader.LoadASCIIFromFile(&model, &err, &warn, fileName.string());
    }

    if (!warn.empty())
        fmt::print("Warning: {}\n", warn);
    if (!err.empty())
        fmt::print("Error: {}\n", err);
    if (!success)
        return false;

    const tinygltf::Scene& scene = model.scenes[model.defaultScene];
    // Load all nodes
    for (auto node : scene.nodes) {
        loadGltfNode(model, model.nodes[node]);
    }

    // Load materials
    for (const auto& mat : model.materials) {
        dp::Material material = {};

        {
            auto glTFPBRMetaillicRoughness = mat.pbrMetallicRoughness;

            // Get the base colour
            material.baseColor.x = static_cast<float>(glTFPBRMetaillicRoughness.baseColorFactor[0]);
            material.baseColor.y = static_cast<float>(glTFPBRMetaillicRoughness.baseColorFactor[1]);
            material.baseColor.z = static_cast<float>(glTFPBRMetaillicRoughness.baseColorFactor[2]);
            // material.baseColor.w = glTFPBRMetaillicRoughness.baseColorFactor[3];

            auto& baseColourTexture = glTFPBRMetaillicRoughness.baseColorTexture;
            if (baseColourTexture.index >= 0) {
                material.baseTextureIndex = baseColourTexture.index;
            }

            // Get the roughness texture
            material.metallicFactor = static_cast<float>(glTFPBRMetaillicRoughness.metallicFactor);
            material.roughnessFactor = static_cast<float>(glTFPBRMetaillicRoughness.roughnessFactor);

            auto& metallicRoughnessTexture = glTFPBRMetaillicRoughness.metallicRoughnessTexture;
            if (metallicRoughnessTexture.index >= 0) {
                material.pbrTextureIndex = metallicRoughnessTexture.index;
            }
        }

        {
            material.emissiveFactor = glm::make_vec3(mat.emissiveFactor.data());

            // Default indices through tinygltf are -1, so we don't have to
            // check anything.
            material.normalTextureIndex = mat.normalTexture.index;
            material.occlusionTextureIndex = mat.occlusionTexture.index;
            material.emissiveTextureIndex = mat.emissiveTexture.index;
        }

        materials.push_back(material);
    }

    // Load textures
    for (const auto& tex : model.textures) {
        dp::TextureFile textureFile;
        const auto& image = model.images[tex.source];
        textureFile.width = image.width;
        textureFile.height = image.height;
        textureFile.format = VK_FORMAT_R8G8B8A8_SRGB;
        textureFile.pixels.resize(image.width * image.height * image.component);
        memcpy(textureFile.pixels.data(), image.image.data(), textureFile.pixels.size());
        textures.emplace_back(textureFile);
    }

    return true;
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

    if (!fileName.has_extension()) {
        fmt::print("File path has no extension.\n");
        return false;
    }
    fs::path ext = fileName.extension();
    bool ret;
    if (ext.compare(".glb") == 0 || ext.compare(".gltf") == 0) {
        ret = loadGltfFile(fileName);
    } else {
        ret = loadAssimpFile(fileName);
    }

    if (!ret) {
        fmt::print("Failed to load file!\n");
        return false;
    }

    fmt::print("Finished loading file!\n");
    return true;
}
