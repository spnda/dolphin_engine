#pragma once

#include <string>

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

    public:
        VmaAllocation allocation;
        VkBuffer handle;
        uint64_t address;

        // Create and allocate a new buffer.
        Buffer(const Context& context);

        Buffer(const Context& context, const std::string name);

        ~Buffer();

        Buffer& operator=(const Buffer& buffer);

        template<typename N, typename M>
        static N alignedSize(N value, M alignment) {
            // static_assert(std::is_base_of<int, N>::value, "N is not derived from int.");
            return (value + alignment - 1) & ~(alignment - 1);
        }

        void create(VkDeviceSize size, VkBufferUsageFlags bufferUsage, VmaMemoryUsage usage, VkMemoryPropertyFlags properties);

        void destroy();

        void createForAccelerationStructure(VkAccelerationStructureBuildSizesInfoKHR buildSizeInfo);

        VkBufferCreateInfo createInfo(VkDeviceSize size, VkBufferUsageFlags bufferUsage);

        VkBufferDeviceAddressInfoKHR getBufferAddressInfo(VkBuffer handle) const;

        /** Gets a basic descriptor buffer info, with given size and given offset, or 0 if omitted. */
        VkDescriptorBufferInfo getDescriptorInfo(const uint64_t size, const uint64_t offset = 0) const;

        VkDeviceOrHostAddressConstKHR getHostAddress();

        /**
         * Copies the memory of size from source into the
         * mapped memory for this buffer.
         */
        void memoryCopy(const void* source, uint64_t size) const;

        /**
         * Copies the memory of size from source into destination.
         * This automatically maps and unmaps memory.
         * TODO: Can throw a access violation reading location 0xFFFFFF.... exception.
         */
        void memoryCopy(void* destination, const void* source, uint64_t size) const;

        void mapMemory(void** destination) const;

        void unmapMemory() const;
    };
} // namespace dp
