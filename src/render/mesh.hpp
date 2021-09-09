#pragma once

#include <glm/glm.hpp>

namespace dp {

    using Index = uint32_t;

    struct Vertex {
        glm::vec3 pos;
    };

    struct Mesh {
        std::string name = {};
        std::vector<Vertex> vertices = {};
        std::vector<Index> indices = {};
        VkTransformMatrixKHR transform;
    };
}
