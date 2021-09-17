#pragma once

#include <glm/glm.hpp>

namespace dp {
    // A index type for the mesh.
    using Index = uint32_t;

    struct Vertex {
        // The relative vertex position.
        // Has to be at the start because of the AS.
        glm::vec3 pos;
    };

    struct Material {
        glm::vec4 diffuse = glm::vec4(1.0f);
        glm::vec4 specular = glm::vec4(1.0f);
        glm::vec4 emissive = glm::vec4(1.0f);
    };

    struct Mesh {
        std::string name = {};
        std::vector<Vertex> vertices = {};
        std::vector<Index> indices = {};
        VkTransformMatrixKHR transform;
        VkFormat vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
        VkIndexType indexType = VK_INDEX_TYPE_UINT32;
        // Typically just the size of a single Vertex.
        static const uint32_t stride = sizeof(Vertex);
        // The material index for the material buffer.
        // We use this together with instanceCustomIndex to provide
        // material data to ray shaders.
        Index materialIndex;
    };
}
