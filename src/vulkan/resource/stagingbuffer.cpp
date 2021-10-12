#include "stagingbuffer.hpp"

dp::StagingBuffer::StagingBuffer(const dp::Context& context)
    : Buffer(context, "stagingBuffer") {
}

dp::StagingBuffer::StagingBuffer(const dp::Context& context, std::string name)
    : Buffer(context, std::move(name)) {
}

void dp::StagingBuffer::create(uint64_t bufferSize) {
    Buffer::create(bufferSize, bufferUsage, memoryUsage, memoryProperties);
}
