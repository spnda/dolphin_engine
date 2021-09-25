#pragma once

#include <mutex>
#include <vulkan/vulkan.h>

namespace dp {
    class Context;

    /** Thread safe vulkan fence */
    class Fence {
        const dp::Context& ctx;
        const std::string name;

        /** Resets and submits must lock/unlock this mutex */
        std::mutex waitMutex;
        VkFence handle;

    public:
        explicit Fence(const dp::Context& context, const std::string name = {});
        Fence(const dp::Fence& fence);

        // operator VkFence() const;
        explicit operator VkFence() const;

        void create(const VkFenceCreateFlags flags = 0);
        void destroy();
        void wait();
        void reset();
    };
}
