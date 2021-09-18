#pragma once

#include <vulkan/vulkan.h>

#include "../context.hpp"

namespace dp {

struct Surface {
    VkSurfaceKHR surface = VK_NULL_HANDLE;

    Surface(const dp::Context& context);
};

} // namespace dp
