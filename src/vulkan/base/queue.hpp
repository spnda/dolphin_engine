#pragma once

#include <mutex>

#include <vulkan/vulkan.h>
#include <VkBootstrap.h>

namespace dp {
    // fwd
    class Context;
    class Fence;

    class Queue {
        const dp::Context& ctx;
        const std::string name;

        mutable std::mutex queueMutex;
        VkQueue handle;
    
    public:
        explicit Queue(const dp::Context& ctx, const std::string name = {});
        explicit Queue(const dp::Context& ctx, const VkQueue queue);
        Queue(const dp::Queue& queue);

        operator VkQueue() const;

        /** Get's the device's VkQueue */
        void getQueue(const vkb::QueueType queueType = vkb::QueueType::graphics);
        void lock() const;
        void unlock() const;
        void waitIdle() const;
        auto submit(const dp::Fence& fence, const VkSubmitInfo* submitInfo) const -> VkResult;
    };
}
