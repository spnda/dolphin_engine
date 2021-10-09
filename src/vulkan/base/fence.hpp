#pragma once

#include <mutex>
#include <vulkan/vulkan.h>

namespace dp {
    class Context;

    class Fence {
        const dp::Context& ctx;
        const std::string name;

        /** Resets and submits must lock/unlock this mutex */
        mutable std::mutex waitMutex = {};
        VkFence handle = nullptr;

    public:
        explicit Fence(const dp::Context& context, std::string name = {});
        Fence(const Fence& fence);

        operator VkFence() const;

        void create(VkFenceCreateFlags flags = 0);
        void destroy() const;
        void wait();
        void reset();
    };
}
