#include "top_level_acceleration_structure.hpp"
#include "bottom_level_acceleration_structure.hpp"

dp::TopLevelAccelerationStructure::TopLevelAccelerationStructure(const dp::Context& context, const VkDeviceAddress instanceAddress, const uint32_t instanceCount)
        : instancesCount(instanceCount), AccelerationStructure(context) {
    instancesData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    instancesData.pNext = nullptr;
	instancesData.arrayOfPointers = VK_FALSE;
	instancesData.data.deviceAddress = instanceAddress;

    topLevelGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    topLevelGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    topLevelGeometry.geometry.instances = instancesData;
	topLevelGeometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
	topLevelGeometry.geometry.instances.arrayOfPointers = VK_FALSE;
	topLevelGeometry.geometry.instances.data.deviceAddress = instanceAddress;

    buildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildGeometryInfo.flags = buildStructureFlags;
    buildGeometryInfo.geometryCount = 1;
    buildGeometryInfo.pGeometries = &topLevelGeometry;
    buildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildGeometryInfo.srcAccelerationStructure = nullptr;

    buildSizesInfo = getBuildSizes(&instanceCount);
}

void dp::TopLevelAccelerationStructure::generate(VkCommandBuffer commandBuffer, const dp::Buffer& scratchBuffer, const dp::Buffer& resultBuffer, const VkDeviceSize resultOffset) {
    build(resultBuffer.handle, resultOffset);

    VkAccelerationStructureBuildRangeInfoKHR buildOffsetInfo = {};
    buildOffsetInfo.primitiveCount = instancesCount;
    std::vector<VkAccelerationStructureBuildRangeInfoKHR*> buildOffsetInfos = { &buildOffsetInfo };

    buildGeometryInfo.dstAccelerationStructure = getAccelerationStructure();
    buildGeometryInfo.scratchData.deviceAddress = scratchBuffer.address; // TODO: offset

    reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(vkGetDeviceProcAddr(context.device.device, "vkCmdBuildAccelerationStructuresKHR"))
        (commandBuffer, 1, &buildGeometryInfo, buildOffsetInfos.data());
}

VkAccelerationStructureInstanceKHR dp::TopLevelAccelerationStructure::createInstance(
        const dp::Context& context,
        const BottomLevelAccelerationStructure& bottomLevelStructure,
        const VkTransformMatrixKHR transformMatrix,
        uint32_t instanceId,
        uint32_t hitGroupId) {
    // TODO: Maybe make this a function of AccelerationStructure?
    VkAccelerationStructureDeviceAddressInfoKHR addressInfo = {};
    addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    addressInfo.accelerationStructure = bottomLevelStructure.getAccelerationStructure();

    const VkDeviceAddress address = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(vkGetDeviceProcAddr(context.device.device, "vkGetAccelerationStructureDeviceAddressKHR"))
        (context.device.device, &addressInfo);

    VkAccelerationStructureInstanceKHR instance = {};
    instance.instanceCustomIndex = instanceId;
    instance.mask = 0xFF;
    instance.instanceShaderBindingTableRecordOffset = hitGroupId;
    instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
    instance.accelerationStructureReference = address;
    instance.transform = transformMatrix;
    return instance;
}
