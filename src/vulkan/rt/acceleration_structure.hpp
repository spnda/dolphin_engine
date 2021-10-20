#pragma once

#include <vector>

#include "../resource/buffer.hpp"
#include "../../models/mesh.hpp"
#include "../resource/stagingbuffer.hpp"

namespace dp {
    // fwd.
    class Context;

    enum class AccelerationStructureType : uint64_t {
        BottomLevel = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        TopLevel = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        Generic = VK_ACCELERATION_STRUCTURE_TYPE_GENERIC_KHR,
    };

    /** The base of any acceleration structure */
    struct AccelerationStructure {
    protected:
        const dp::Context& ctx;

    public:
        dp::Buffer resultBuffer;
        dp::Buffer scratchBuffer;
        dp::AccelerationStructureType type = AccelerationStructureType::Generic;
        VkDeviceAddress address = 0;
        VkAccelerationStructureKHR handle = nullptr;

        explicit AccelerationStructure(const dp::Context& ctx, dp::AccelerationStructureType type, std::string name);
        AccelerationStructure(const AccelerationStructure& as) = default;

        void buildStructure(VkCommandBuffer cmdBuffer, uint32_t geometryCount,
                            VkAccelerationStructureBuildGeometryInfoKHR& geometryInfo, VkAccelerationStructureBuildRangeInfoKHR** rangeInfo);
        void createScratchBuffer(VkAccelerationStructureBuildSizesInfoKHR buildSizes);
        void createResultBuffer(VkAccelerationStructureBuildSizesInfoKHR buildSizes);
        void createStructure(VkAccelerationStructureBuildSizesInfoKHR buildSizes);
        void destroy();
        /** Returns already aligned build sizes. */
        auto getBuildSizes(uint64_t primitiveCount,
                           VkAccelerationStructureBuildGeometryInfoKHR* buildGeometryInfo,
                           VkPhysicalDeviceAccelerationStructurePropertiesKHR asProperties) -> VkAccelerationStructureBuildSizesInfoKHR;
        auto getDescriptorWrite() const -> VkWriteDescriptorSetAccelerationStructureKHR;
    };

    struct BottomLevelAccelerationStructure final : public AccelerationStructure {
        dp::Mesh mesh;
        dp::StagingBuffer transformStagingBuffer;
        dp::StagingBuffer meshStagingBuffer;

    public:
        dp::Buffer transformBuffer;
        dp::Buffer meshBuffer;
        uint64_t vertexOffset;
        uint64_t indexOffset;

        explicit BottomLevelAccelerationStructure(const dp::Context& ctx, const dp::Mesh& mesh);

        void createMeshBuffers(VkPhysicalDeviceAccelerationStructurePropertiesKHR asProperties);
        void copyMeshBuffers(VkCommandBuffer cmdBuffer);
        void destroyMeshBuffers();
    };

    struct TopLevelAccelerationStructure final : public AccelerationStructure {
    public:
        explicit TopLevelAccelerationStructure(const dp::Context& ctx);
    };
}
