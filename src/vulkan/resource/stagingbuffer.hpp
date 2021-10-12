#pragma once

#include "buffer.hpp"

namespace dp {
    class StagingBuffer : public Buffer {
        static const VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY;
        static const VkBufferUsageFlags bufferUsage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        static const VkMemoryPropertyFlags memoryProperties = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

    public:
        explicit StagingBuffer(const dp::Context& context);
        explicit StagingBuffer(const dp::Context& context, std::string name);
        StagingBuffer(const StagingBuffer& buffer) = default;

        void create(uint64_t bufferSize);
    };
}
