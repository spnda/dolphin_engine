#pragma once

#include <string>
#include <mutex>

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace dp {
    class Context;

    /**
     * A basic data buffer, managed using vma.
     */
    class Buffer {
        const Context& context;
        std::string name;

        mutable std::mutex memoryMutex;

    public:
        VmaAllocation allocation;
        VkBuffer handle;
        uint64_t address;

        // Create and allocate a new buffer.
        Buffer(const Context& context);
        Buffer(const Context& context, const std::string name);
        Buffer(const Buffer& buffer);

        ~Buffer();

        Buffer& operator=(const Buffer& buffer);

        static auto alignedSize(size_t value, size_t alignment) -> size_t;

        void create(VkDeviceSize size, VkBufferUsageFlags bufferUsage, VmaMemoryUsage usage, VkMemoryPropertyFlags properties);
        void destroy();

        void createForAccelerationStructure(VkAccelerationStructureBuildSizesInfoKHR buildSizeInfo);

        VkBufferCreateInfo createInfo(VkDeviceSize size, VkBufferUsageFlags bufferUsage);
        VkBufferDeviceAddressInfoKHR getBufferAddressInfo(VkBuffer handle) const;
        /** Gets a basic descriptor buffer info, with given size and given offset, or 0 if omitted. */
        VkDescriptorBufferInfo getDescriptorInfo(const uint64_t size, const uint64_t offset = 0) const;
        VkDeviceOrHostAddressConstKHR getHostAddress();

        void lock() const;
        void unlock() const;

        /**
         * Copies the memory of size from source into the
         * mapped memory for this buffer.
         */
        void memoryCopy(const void* source, uint64_t size) const;

        /**
         * Copies the memory of size from source into destination.
         * This automatically maps and unmaps memory. The caller
         * has to lock/unlock the mutex themselves.
         */
        void memoryCopy(void* destination, const void* source, uint64_t size) const;

        void mapMemory(void** destination) const;
        void unmapMemory() const;
    };
} // namespace dp
