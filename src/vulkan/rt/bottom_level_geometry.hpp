#pragma once

#include <vector>

#include <vulkan/vulkan.h>

#include "../resource/buffer.hpp"

namespace dp {
    // A collection of bottom level geometry
    class BottomLevelGeometry {
    public:
        // Get the size of underlying geometry vector.
        // Alias for getGeometry().size()
        uint32_t size() const;

        const std::vector<VkAccelerationStructureGeometryKHR>& getGeometry() const;
        const std::vector<VkAccelerationStructureBuildRangeInfoKHR>& getBuildOffsetInfos() const;

        void addGeometryTriangles(dp::Buffer& vertexBuffer, uint32_t vertexCount, dp::Buffer& indexBuffer, uint32_t indexCount, bool isOpaque);

    private:
        std::vector<VkAccelerationStructureGeometryKHR> structureGeometries;
        std::vector<VkAccelerationStructureBuildRangeInfoKHR> buildOffsetInfos;
    };
}

