#pragma once

#include <vulkan/vulkan.h>

#include "VkBootstrap.h"

#include "instance.hpp"
#include "physical_device.hpp"

namespace dp {
    class Device {
        vkb::Device device = {};

    public:
        explicit Device() = default;
        Device(const Device&) = default;
        Device& operator=(const Device&) = default;

        void create(const dp::PhysicalDevice& physicalDevice);
        void destroy() const;
        [[nodiscard]] VkQueue getQueue(vkb::QueueType queueType) const;
        [[nodiscard]] uint32_t getQueueIndex(vkb::QueueType queueType) const;

        template<class T>
        T getFunctionAddress(const std::string& functionName) const {
            return reinterpret_cast<T>(vkGetDeviceProcAddr(device, functionName.c_str()));
        }

        explicit operator vkb::Device() const;
        operator VkDevice() const;
    };
} // namespace dp
