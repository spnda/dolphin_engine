#pragma once

#include <vulkan/vulkan.h>

#include "VkBootstrap.h"

#include "surface.hpp"

namespace dp {

const std::vector<const char*> deviceExtensions = {
    VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
    VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,

    VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
    VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
    VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
    VK_KHR_SPIRV_1_4_EXTENSION_NAME, // Very important! Our shaders are compiled to SPV1.4 and will not work without.
    VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME,
};

class VulkanInstance;

class Device {
private:
    vkb::PhysicalDevice physicalDevice = {};
    vkb::Device device = {};

public:
    Device(vkb::Instance &instance, Surface &surface);

    static vkb::PhysicalDevice getPhysicalDevice(vkb::Instance instance, VkSurfaceKHR surface);

    static vkb::Device getLogicalDevice(VkInstance instance, vkb::PhysicalDevice physicalDevice);

    static VkCommandPool createDefaultCommandPool(vkb::Device device, uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags);

    vkb::Device getDevice();

    // Wait for the device to be idle.
    // Note: Locks up thread.
    void waitForIdle();
};

} // namespace dp
