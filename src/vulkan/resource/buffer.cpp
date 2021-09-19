#include <vk_mem_alloc.h>

#include "../context.hpp"
#include "buffer.hpp"
#include "../utils.hpp"

dp::Buffer::Buffer(const dp::Context& context)
        : context(context), name("") {
}

dp::Buffer::Buffer(const dp::Context& context, const std::string name)
        : context(context), name(name) {

}

dp::Buffer::~Buffer() {}

dp::Buffer& dp::Buffer::operator=(const dp::Buffer& buffer) {
    this->address = buffer.address;
    this->allocation = buffer.allocation;
    this->handle = buffer.handle;
    this->name = buffer.name;
    return *this;
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
        address = reinterpret_cast<PFN_vkGetBufferDeviceAddressKHR>(vkGetDeviceProcAddr(context.device, "vkGetBufferDeviceAddressKHR"))
            (context.device, &addressInfo);
    }

    if (!name.empty()) {
        context.setDebugUtilsName(handle, name);
    }
}

void dp::Buffer::destroy() {
    vmaDestroyBuffer(context.vmaAllocator, handle, allocation);
    handle = VK_NULL_HANDLE;
}

void dp::Buffer::createForAccelerationStructure(VkAccelerationStructureBuildSizesInfoKHR buildSizeInfo) {
    // We sadly cannot use VMA for AS buffer's, as it is an extension
    // and only support for Vulkan core components is implemented.
    VkBufferCreateInfo bufferCreateInfo = {};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.size = buildSizeInfo.accelerationStructureSize;
    bufferCreateInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    vkCreateBuffer(context.device, &bufferCreateInfo, nullptr, &handle);
    
    VkMemoryRequirements memoryRequirements = {};
    vkGetBufferMemoryRequirements(context.device, handle, &memoryRequirements);

    VmaAllocationCreateInfo allocationCreateInfo = {};
    allocationCreateInfo.memoryTypeBits = memoryRequirements.memoryTypeBits;
    vmaAllocateMemory(context.vmaAllocator, &memoryRequirements, &allocationCreateInfo, &allocation, nullptr);

    vmaBindBufferMemory(context.vmaAllocator, allocation, handle);

    auto addressInfo = getBufferAddressInfo(handle);
    address = vkGetBufferDeviceAddress(context.device, &addressInfo);
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

VkBufferDeviceAddressInfoKHR dp::Buffer::getBufferAddressInfo(VkBuffer handle) const {
    VkBufferDeviceAddressInfoKHR bufferDeviceAddressInfo = {};
    bufferDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    bufferDeviceAddressInfo.buffer = handle;
    return bufferDeviceAddressInfo;
}

VkDescriptorBufferInfo dp::Buffer::getDescriptorInfo(const uint64_t size, const uint64_t offset) const {
    VkDescriptorBufferInfo info = {};
    info.buffer = handle;
    info.offset = offset;
    info.range = size;
    return info;
}

VkDeviceOrHostAddressConstKHR dp::Buffer::getHostAddress() {
    return { .deviceAddress = address, };
}

void dp::Buffer::memoryCopy(const void* source, uint64_t size) const {
    void* dst;
    this->mapMemory(&dst);
    memcpy(dst, source, size);
    this->unmapMemory();
}

void dp::Buffer::memoryCopy(void* destination, const void* source, uint64_t size) const {
    memcpy(destination, source, size);
}

void dp::Buffer::mapMemory(void** destination) const {
    vmaMapMemory(context.vmaAllocator, allocation, destination);
}

void dp::Buffer::unmapMemory() const {
    vmaUnmapMemory(context.vmaAllocator, allocation);
}
