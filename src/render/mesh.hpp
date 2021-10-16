#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace dp {
    // An index type for the mesh.
    using Index = uint32_t;

    struct Vertex {
        // The relative vertex position.
        // Has to be at the start because of the AS.
        glm::fvec4 pos;
        glm::fvec4 normals;
        glm::fvec2 uv;
        glm::fvec2 padding = glm::fvec2(1.0);
    };

    /**
     * Struct referencing buffer addresses for various mesh related buffers.
     * Used for the shader to obtain vertex data per mesh.
     */
    struct ObjectDescription {
        VkDeviceAddress vertexBufferAddress;
        VkDeviceAddress indexBufferAddress;
        Index materialIndex;
    };

    struct Material {
        glm::vec4 diffuse = glm::vec4(1.0f);
        glm::vec4 specular = glm::vec4(1.0f);
        glm::vec4 emissive = glm::vec4(1.0f);
        int32_t textureIndex = -1; // Negative values represent invalid or no texture.
    };

    struct Mesh {
        std::string name = {};
        std::vector<Vertex> vertices = {};
        std::vector<Index> indices = {};
        VkTransformMatrixKHR transform;
        VkFormat vertexFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
        VkIndexType indexType = VK_INDEX_TYPE_UINT32;
        // Typically, just the size of a single Vertex.
        static const uint32_t vertexStride = sizeof(Vertex);
        // The material index for the material buffer.
        // We use this together with instanceCustomIndex to provide
        // material data to ray shaders.
        Index materialIndex;
    };
}
