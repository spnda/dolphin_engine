#pragma once

#include <vulkan/vulkan.h>

#include "../context.hpp"

namespace dp {

struct Surface {
    VkSurfaceKHR surface = VK_NULL_HANDLE;

    Surface(Context& context);
};

} // namespace dp
