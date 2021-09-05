#include <vk_mem_alloc.h>

#include "buffer.hpp"

dp::Buffer::Buffer(const dp::Context& context)
        : context(context), name("") {
}

dp::Buffer::Buffer(const dp::Context& context, const std::string name)
        : context(context), name(name) {

}

void dp::Buffer::create(VkDeviceSize size, VkBufferUsageFlags bufferUsage, VmaMemoryUsage usage, VkMemoryPropertyFlags properties) {
    VkBufferCreateInfo bufferCreateInfo = createInfo(size, bufferUsage);
        
    VmaAllocationCreateInfo allocationInfo = {};
    allocationInfo.usage = usage;
    allocationInfo.requiredFlags = properties;

    vmaCreateBuffer(context.vmaAllocator, &bufferCreateInfo, &allocationInfo, &handle, &allocation, nullptr);
    assert(allocation != nullptr);

    if (isFlagSet(bufferUsage, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)) {
        auto addressInfo = getBufferAddressInfo(handle);
        address = vkGetBufferDeviceAddress(context.device.device, &addressInfo);
    }

    if (!name.empty()) {
        context.setDebugUtilsName(handle, name);
    }
}

void dp::Buffer::createForAccelerationStructure(VkAccelerationStructureBuildSizesInfoKHR buildSizeInfo) {
    // We sadly cannot use VMA for AS buffer's, as it is an extension
    // and only support for Vulkan core components is implemented.
    VkBufferCreateInfo bufferCreateInfo = {};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.size = buildSizeInfo.accelerationStructureSize;
    bufferCreateInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    vkCreateBuffer(context.device.device, &bufferCreateInfo, nullptr, &handle);
    
    VkMemoryRequirements memoryRequirements = {};
    vkGetBufferMemoryRequirements(context.device.device, handle, &memoryRequirements);

    VmaAllocationCreateInfo allocationCreateInfo = {};
    allocationCreateInfo.memoryTypeBits = memoryRequirements.memoryTypeBits;
    vmaAllocateMemory(context.vmaAllocator, &memoryRequirements, &allocationCreateInfo, &allocation, nullptr);

    vmaBindBufferMemory(context.vmaAllocator, allocation, handle);

    auto addressInfo = getBufferAddressInfo(handle);
    address = vkGetBufferDeviceAddress(context.device.device, &addressInfo);
}

VkBufferCreateInfo dp::Buffer::createInfo(VkDeviceSize size, VkBufferUsageFlags bufferUsage) {
    VkBufferCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.pNext = nullptr;

    info.size = size;
    info.usage = bufferUsage;
    //info.usage = bufferUsage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT; // We automatically set this bit just to be sure.
    return info;
}

VkBufferDeviceAddressInfoKHR dp::Buffer::getBufferAddressInfo(VkBuffer handle) {
    VkBufferDeviceAddressInfoKHR bufferDeviceAddressInfo = {};
    bufferDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    bufferDeviceAddressInfo.buffer = handle;
    return bufferDeviceAddressInfo;
}

void dp::Buffer::memoryCopy(const void* source, uint64_t size) {
    void* dst;
    this->mapMemory(&dst);
    memcpy(dst, source, size);
    this->unmapMemory();
}

void dp::Buffer::memoryCopy(void* destination, const void* source, uint64_t size) {
    this->mapMemory(&destination);
    memcpy(destination, source, size);
    this->unmapMemory();
}

void dp::Buffer::mapMemory(void** destination) {
    vmaMapMemory(context.vmaAllocator, allocation, destination);
}

void dp::Buffer::unmapMemory() {
    vmaUnmapMemory(context.vmaAllocator, allocation);
}
