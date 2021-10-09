#include "buffer.hpp"
#include <vk_mem_alloc.h>

#include "../context.hpp"
#include "../utils.hpp"

dp::Buffer::Buffer(const dp::Context& context)
        : context(context) {
}

dp::Buffer::Buffer(const dp::Context& context, std::string  name)
        : context(context), name(std::move(name)) {
}

dp::Buffer::Buffer(const dp::Buffer& buffer)
        : context(buffer.context), name(buffer.name),
          allocation(buffer.allocation), handle(buffer.handle), address(buffer.address) {
    
}

dp::Buffer& dp::Buffer::operator=(const dp::Buffer& buffer) {
    this->address = buffer.address;
    this->allocation = buffer.allocation;
    this->handle = buffer.handle;
    this->name = buffer.name;
    return *this;
}

auto dp::Buffer::alignedSize(size_t value, size_t alignment) -> size_t {
    return (value + alignment - 1) & -alignment;
}

void dp::Buffer::create(const VkDeviceSize newSize, const VkBufferUsageFlags bufferUsage, const VmaMemoryUsage usage, const VkMemoryPropertyFlags properties) {
    this->size = newSize;
    auto bufferCreateInfo = getCreateInfo(bufferUsage);

    VmaAllocationCreateInfo allocationInfo = {
        .usage = usage,
        .requiredFlags = properties,
    };

    auto result = vmaCreateBuffer(context.vmaAllocator, &bufferCreateInfo, &allocationInfo, &handle, &allocation, nullptr);
    checkResult(context, result, "Failed to create buffer \"" + name + "\"");
    assert(allocation != nullptr);

    if (isFlagSet(bufferUsage, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)) {
        auto addressInfo = getBufferAddressInfo(handle);
        address = context.getBufferDeviceAddress(addressInfo);
    }

    if (!name.empty()) {
        context.setDebugUtilsName(handle, name);
    }
}

void dp::Buffer::destroy() {
    if (handle == nullptr || allocation == nullptr) return;
    vmaDestroyBuffer(context.vmaAllocator, handle, allocation);
    handle = nullptr;
}

void dp::Buffer::lock() const {
    memoryMutex.lock();
}

void dp::Buffer::unlock() const {
    memoryMutex.unlock();
}

auto dp::Buffer::getCreateInfo(VkBufferUsageFlags bufferUsage) const -> VkBufferCreateInfo {
    return {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = bufferUsage,
    };
}

auto dp::Buffer::getBufferAddressInfo(VkBuffer handle) -> VkBufferDeviceAddressInfoKHR {
    return {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = handle,
    };
}

auto dp::Buffer::getDescriptorInfo(const uint64_t bufferRange, const uint64_t offset) const -> VkDescriptorBufferInfo {
    return {
        .buffer = handle,
        .offset = offset,
        .range = bufferRange,
    };
}

auto dp::Buffer::getHandle() const -> const VkBuffer {
    return this->handle;
}

auto dp::Buffer::getHostAddress() const -> VkDeviceOrHostAddressConstKHR {
    return { .deviceAddress = address, };
}

auto dp::Buffer::getSize() const -> VkDeviceSize {
    return size;
}

void dp::Buffer::memoryCopy(const void* source, uint64_t copySize) const {
    std::lock_guard guard(memoryMutex);
    void* dst;
    this->mapMemory(&dst);
    memcpy(dst, source, copySize);
    this->unmapMemory();
}

void dp::Buffer::memoryCopy(void* destination, const void* source, uint64_t size) {
    memcpy(destination, source, size);
}

void dp::Buffer::mapMemory(void** destination) const {
    vmaMapMemory(context.vmaAllocator, allocation, destination);
}

void dp::Buffer::unmapMemory() const {
    vmaUnmapMemory(context.vmaAllocator, allocation);
}
