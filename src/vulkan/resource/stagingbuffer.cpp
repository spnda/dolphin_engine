#include "stagingbuffer.hpp"

dp::StagingBuffer::StagingBuffer(const dp::Context& context, std::string name)
    : Buffer(context, std::move(name)) {
}

void dp::StagingBuffer::create(uint64_t bufferSize, VkBufferUsageFlags additionalBufferUsage) {
    Buffer::create(bufferSize, bufferUsage | additionalBufferUsage, memoryUsage, memoryProperties);
}
