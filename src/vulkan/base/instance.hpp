#pragma once

#include <string>
#include <vector>

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include "VkBootstrap.h"

namespace dp {
    // fwd
    class Window;

    class VulkanInstance {
        const std::vector<const char*> requiredExtensions = {};

        // Creates a new SDL2 window instance and creates a VkInstance.
        VulkanInstance();

    public:
        Window* window              = VK_NULL_HANDLE;
        vkb::Instance vkInstance    = {};

        static vkb::Instance buildInstance(const std::string name, const uint32_t version, const std::vector<const char*> extensions);

        VulkanInstance(VulkanInstance const&) = delete;
        void operator=(VulkanInstance const&) = delete;
    };
} // namespace dp
