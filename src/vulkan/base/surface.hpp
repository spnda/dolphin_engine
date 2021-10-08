#pragma once

#include <vulkan/vulkan.h>

namespace dp {
    // fwd
    class Context;

    struct Surface {
        VkSurfaceKHR surface = VK_NULL_HANDLE;

        explicit Surface(const dp::Context& context);
    };
} // namespace dp
