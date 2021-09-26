#pragma once

#include <vulkan/vulkan.h>

#include "VkBootstrap.h"

namespace dp {
    const std::vector<const char*> deviceExtensions = {
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,

        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
        VK_KHR_SPIRV_1_4_EXTENSION_NAME, // Very important! Our shaders are compiled to SPV1.4 and will not work without.
        VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME,
        VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,
    };

    // fwd
    class Surface;

    class Device {
        vkb::PhysicalDevice physicalDevice = {};
        vkb::Device device = {};

    public:
        Device(const vkb::Instance& instance, Surface &surface);

        /**
         * Get the physical device per vkb::PhysicalDeviceSelector from
         * the given instance and surface.
         */
        static vkb::PhysicalDevice getPhysicalDevice(const vkb::Instance& instance, const VkSurfaceKHR& surface);

        /**
         * Get the logical device for the given physical device.
         */
        static vkb::Device getLogicalDevice(const vkb::Instance& instance, const vkb::PhysicalDevice& physicalDevice);

        vkb::Device& getDevice();
    };
} // namespace dp
