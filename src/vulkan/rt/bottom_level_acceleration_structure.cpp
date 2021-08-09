#include "bottom_level_acceleration_structure.hpp"
#include "bottom_level_geometry.hpp"

dp::BottomLevelAccelerationStructure::BottomLevelAccelerationStructure(const dp::Context& context, const dp::BottomLevelGeometry& geometry)
        : AccelerationStructure(context), geometries(std::move(geometry)) {
    buildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildGeometryInfo.flags = buildStructureFlags;
    buildGeometryInfo.geometryCount = geometries.size();
    buildGeometryInfo.pGeometries = geometries.getGeometry().data();
    buildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    buildGeometryInfo.srcAccelerationStructure = nullptr;

    std::vector<uint32_t> maxPrimitiveCount(geometries.getBuildOffsetInfos().size());
    for (size_t i = 0; i < maxPrimitiveCount.size(); ++i) {
        maxPrimitiveCount[i] = geometries.getBuildOffsetInfos()[i].primitiveCount;
    }
    buildSizesInfo = getBuildSizes(maxPrimitiveCount.data());
}

void dp::BottomLevelAccelerationStructure::generate(VkCommandBuffer commandBuffer, dp::Buffer& scratchBuffer, dp::Buffer& resultBuffer, const VkDeviceSize resultOffset) {
    build(resultBuffer.handle, resultOffset);

    buildGeometryInfo.dstAccelerationStructure = getAccelerationStructure();
    buildGeometryInfo.scratchData.deviceAddress = scratchBuffer.address;

    const VkAccelerationStructureBuildRangeInfoKHR* buildOffsetInfos = geometries.getBuildOffsetInfos().data();

    reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(vkGetDeviceProcAddr(context.device.device, "vkCmdBuildAccelerationStructuresKHR"))
        (commandBuffer, 1, &buildGeometryInfo, &buildOffsetInfos);
}
