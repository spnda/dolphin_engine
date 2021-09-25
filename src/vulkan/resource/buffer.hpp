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
        VmaAllocation allocation = nullptr;
        VkDeviceSize size = 0;
        VkBuffer handle = nullptr;

        auto getCreateInfo(VkBufferUsageFlags bufferUsage) const -> VkBufferCreateInfo;
        auto getBufferAddressInfo(VkBuffer handle) const -> VkBufferDeviceAddressInfoKHR;

    public:
        VkDeviceAddress address = 0;

        // Create and allocate a new buffer.
        Buffer(const Context& context);
        Buffer(const Context& context, const std::string name);
        Buffer(const Buffer& buffer);
        ~Buffer() = default;

        Buffer& operator=(const Buffer& buffer);

        static auto alignedSize(size_t value, size_t alignment) -> size_t;

        void create(const VkDeviceSize size, const VkBufferUsageFlags bufferUsage, const VmaMemoryUsage usage, const VkMemoryPropertyFlags properties);
        void destroy();
        void lock() const;
        void unlock() const;

        /** Gets a basic descriptor buffer info, with given size and given offset, or 0 if omitted. */
        auto getDescriptorInfo(const uint64_t size, const uint64_t offset = 0) const -> VkDescriptorBufferInfo;
        auto getHandle() const -> const VkBuffer;
        auto getHostAddress() const -> VkDeviceOrHostAddressConstKHR;
        auto getSize() const -> VkDeviceSize;

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
