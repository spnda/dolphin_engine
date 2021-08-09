#pragma once

#include <string>
#include <vector>

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include "VkBootstrap.h"

#include "../../sdl/window.hpp"
#include "device.hpp"
#include "surface.hpp"

namespace dp {

class VulkanInstance {
private:
    const std::vector<const char*> requiredExtensions = {
        
    };

    // Creates a new SDL2 window instance and creates a VkInstance.
    VulkanInstance();

public:
    Window* window              = VK_NULL_HANDLE;
    vkb::Instance vkInstance    = {};

    Device* aqcuireDevice(dp::Surface surface) {
        return new Device(this->vkInstance, surface);
    }

    static vkb::Instance buildInstance(std::string name, int version, std::vector<const char*> extensions);

    VulkanInstance(VulkanInstance const&) = delete;
    void operator=(VulkanInstance const&) = delete;
};

} // namespace dp
