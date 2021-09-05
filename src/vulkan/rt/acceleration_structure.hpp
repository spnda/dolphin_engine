#pragma once

#include "../context.hpp"
#include "../resource/buffer.hpp"

namespace dp {
    class AccelerationStructure {
    public:
        virtual ~AccelerationStructure();

        VkAccelerationStructureKHR getAccelerationStructure() const;
        const VkAccelerationStructureBuildSizesInfoKHR getBuildSizes() const;

        static void memoryBarrier(const VkCommandBuffer commandBuffer);

    protected:
        const dp::Context& context;

        // TODO: Make this configurable.
        const VkBuildAccelerationStructureFlagsKHR buildStructureFlags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;

        VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo = {};
        VkAccelerationStructureBuildSizesInfoKHR buildSizesInfo = {};

        explicit AccelerationStructure(const dp::Context& context);

        VkAccelerationStructureBuildSizesInfoKHR getBuildSizes(const uint32_t* primitiveCount) const;

        void build(const VkBuffer resultBuffer, const VkDeviceSize resultOffset);

    private:
        VkAccelerationStructureKHR accelerationStructure;
    };
}
