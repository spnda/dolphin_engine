#pragma once

#include <mutex>

#include <vulkan/vulkan.h>
#include <VkBootstrap.h>

namespace dp {
    // fwd
    class Context;
    class Fence;
    class Semaphore;

    class Queue {
        PFN_vkQueuePresentKHR vkQueuePresent = nullptr;

        const dp::Context& ctx;
        const std::string name;

        mutable std::mutex queueMutex = {};
        VkQueue handle = nullptr;

    public:
        explicit Queue(const dp::Context& ctx, std::string name = {});
        Queue(const dp::Queue& queue);

        operator VkQueue() const;

        /** Get's the device's VkQueue */
        void create(vkb::QueueType queueType = vkb::QueueType::graphics);
        void lock() const;
        void unlock() const;
        void waitIdle() const;

        /** Creates a new unique_lock, which will automatically lock the mutex. */
        [[nodiscard]] auto getLock() const -> std::unique_lock<std::mutex>;
        [[nodiscard]] auto submit(const dp::Fence& fence, const VkSubmitInfo* submitInfo) const -> VkResult;
        [[nodiscard]] auto present(uint32_t imageIndex, const VkSwapchainKHR& swapchain, const dp::Semaphore& waitSemaphore) const -> VkResult;
    };
}
