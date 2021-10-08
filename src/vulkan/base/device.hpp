#pragma once

#include <vulkan/vulkan.h>

#include "VkBootstrap.h"

namespace dp {
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

        static VkCommandPool createDefaultCommandPool(const vkb::Device& device, uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags);

        vkb::Device& getHandle();
    };
} // namespace dp
