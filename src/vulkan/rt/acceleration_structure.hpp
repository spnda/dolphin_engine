#pragma once

#include <string>
#include <mutex>
#include <vulkan/vulkan.h>

#include "../resource/buffer.hpp"
#include "../../render/mesh.hpp"

namespace dp {
    // fwd
    class Context;
    
    enum AccelerationStructureType : uint32_t {
        TopLevel = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        BottomLevel = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        Generic = VK_ACCELERATION_STRUCTURE_TYPE_GENERIC_KHR
    };

    enum AccelerationStructureBuildMode : uint32_t {
        Build = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        Update = VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR
    };

    class AccelerationStructure {
    protected:
        const dp::Context& ctx;
        std::string name;

        dp::Buffer scratchBuffer;
        dp::Buffer resultBuffer;

        std::mutex writeMutex;

        VkAccelerationStructureGeometryKHR geometry = {};
        VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo = {};
        VkAccelerationStructureBuildSizesInfoKHR buildSizes = {};
        uint32_t primitiveCount = 0;

        VkAccelerationStructureKHR previousHandle = nullptr;
        VkAccelerationStructureKHR handle = nullptr;

        explicit AccelerationStructure(const dp::Context& context, const AccelerationStructureType type, const std::string asName);
        virtual ~AccelerationStructure() = default;

        void createBuildBuffers(dp::Buffer& scratchBuffer, dp::Buffer& resultBuffer) const;

        auto getAccelerationStructureProperties() const -> VkPhysicalDeviceAccelerationStructurePropertiesKHR;

    public:
        dp::AccelerationStructureType type = AccelerationStructureType::Generic;
        VkDeviceAddress address = 0;
        
        AccelerationStructure(const AccelerationStructure& structure);

        AccelerationStructure& operator=(const dp::AccelerationStructure& structure);

        void setName();
        auto getHandle() const -> const VkAccelerationStructureKHR&;
        auto getPreviousHandle() const -> const VkAccelerationStructureKHR&;

        /** Creates the AS */
        virtual void create(const dp::AccelerationStructureBuildMode mode = Build) = 0;

        /** Builds the AS on the graphics queue */
        void build(const VkCommandBuffer commandBuffer);

        void destroy();
    };

    class BottomLevelAccelerationStructure : public AccelerationStructure {
        dp::Buffer vertexBuffer;
        dp::Buffer indexBuffer;
        dp::Buffer transformBuffer;
        dp::Mesh mesh;

        /** Creates and fills the mesh buffers above */
        void createMeshBuffers();

    public:
        explicit BottomLevelAccelerationStructure(const dp::Context& context, dp::Mesh mesh, const std::string name = "blas");
        
        auto getMesh() const -> const dp::Mesh&;

        void create(const dp::AccelerationStructureBuildMode mode = Build) override;
        void cleanup();
    };

    class TopLevelAccelerationStructure : public AccelerationStructure {
        std::vector<BottomLevelAccelerationStructure> blases = {};
        std::vector<VkAccelerationStructureInstanceKHR> instances = {};

        dp::Buffer instanceBuffer;

    public:
        explicit TopLevelAccelerationStructure(const dp::Context& context, const std::string name = "tlas");
    
        void create(const dp::AccelerationStructureBuildMode mode = Build) override;

        /** Destroys build/create relevant buffers and handles that are no longer needed. */
        void cleanup();

        void updateBLASes(const std::vector<BottomLevelAccelerationStructure>& newBlases);
    };
}
