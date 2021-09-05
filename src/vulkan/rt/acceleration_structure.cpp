#include "acceleration_structure.hpp"

uint64_t roundUp(uint64_t size, uint64_t granularity) {
	const auto divUp = (size + granularity - 1) / granularity;
	return divUp * granularity;
}

dp::AccelerationStructure::~AccelerationStructure() {
	if (accelerationStructure != nullptr && false) { // Object lifetime is a shit thing to handle.
        reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(vkGetDeviceProcAddr(context.device.device, "vkDestroyAccelerationStructureKHR"))
		    (context.device.device, accelerationStructure, nullptr);
		accelerationStructure = nullptr;
	}
}

VkAccelerationStructureKHR dp::AccelerationStructure::getAccelerationStructure() const {
    return accelerationStructure;
}

const VkAccelerationStructureBuildSizesInfoKHR dp::AccelerationStructure::getBuildSizes() const {
    return buildSizesInfo;
}

dp::AccelerationStructure::AccelerationStructure(const dp::Context& context) : context(context) {

}

VkAccelerationStructureBuildSizesInfoKHR dp::AccelerationStructure::getBuildSizes(const uint32_t* primitiveCount) const {
    VkAccelerationStructureBuildSizesInfoKHR sizeInfo = {};
    sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

    reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(vkGetDeviceProcAddr(context.device.device, "vkGetAccelerationStructureBuildSizesKHR"))
        (context.device.device,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildGeometryInfo,
        primitiveCount,
        &sizeInfo);
    
    // Apparently a acceleration structure has to be aligned by 256 bytes?
    // TODO: Improve this. See https://github.com/GPSnoopy/RayTracingInVulkan/blob/master/src/Vulkan/RayTracing/AccelerationStructure.cpp#L13
    static const uint64_t accelerationStructureAlignment = 256;
    static const uint64_t scratchBufferAlignment = 128; // This is just my card, RTX 2080. Might differ from other cards.
    // sizeInfo.accelerationStructureSize = roundUp(sizeInfo.accelerationStructureSize, accelerationStructureAlignment);
    sizeInfo.buildScratchSize = roundUp(sizeInfo.buildScratchSize, scratchBufferAlignment);
    return sizeInfo;
}

void dp::AccelerationStructure::build(const VkBuffer resultBuffer, const VkDeviceSize resultOffset) {
    VkAccelerationStructureCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createInfo.pNext = nullptr;
    createInfo.type = buildGeometryInfo.type;
    createInfo.size = getBuildSizes().accelerationStructureSize;
    createInfo.buffer = resultBuffer;
    createInfo.offset = resultOffset;

    reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(vkGetDeviceProcAddr(context.device.device, "vkCreateAccelerationStructureKHR"))
        (context.device.device, &createInfo, nullptr, &accelerationStructure);
}

void dp::AccelerationStructure::memoryBarrier(const VkCommandBuffer commandBuffer) {
    VkMemoryBarrier memoryBarrier = {};
	memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	memoryBarrier.pNext = nullptr;
	memoryBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
	memoryBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

	vkCmdPipelineBarrier(
		commandBuffer,
		VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
		VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
		0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
}
