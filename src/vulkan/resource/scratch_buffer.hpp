#pragma once

#include "buffer.hpp"

namespace dp {

class ScratchBuffer {
private:
    dp::Buffer buffer;
    
public:
    ScratchBuffer(Context context)
        : buffer(context) {
    };

    ~ScratchBuffer() {
        buffer.~Buffer(); // Deconstruct the buffer manually.
    }

    void create(VkDeviceSize size) {
        buffer.create(size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    }

    VkDeviceAddress getDeviceAddress() {
        return buffer.address;
    }
};

} // namespace dp
