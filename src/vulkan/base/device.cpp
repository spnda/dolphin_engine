#include "device.hpp"

#include "../utils.hpp"

void dp::Device::create(const dp::PhysicalDevice& physicalDevice) {
    vkb::DeviceBuilder deviceBuilder((vkb::PhysicalDevice(physicalDevice)));
    auto deviceDiagnosticsConfigCreateInfo = VkDeviceDiagnosticsConfigCreateInfoNV {
        .sType = VK_STRUCTURE_TYPE_DEVICE_DIAGNOSTICS_CONFIG_CREATE_INFO_NV,
        .flags = VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_SHADER_DEBUG_INFO_BIT_NV | VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_AUTOMATIC_CHECKPOINTS_BIT_NV,
    };
    deviceBuilder.add_pNext(&deviceDiagnosticsConfigCreateInfo);
    device = getFromVkbResult(deviceBuilder.build());
}

void dp::Device::destroy() const {
    vkb::destroy_device(device);
}

VkResult dp::Device::waitIdle() const {
    return vkDeviceWaitIdle(device);
}

VkQueue dp::Device::getQueue(const vkb::QueueType queueType) const {
    return getFromVkbResult(device.get_queue(queueType));
}

uint32_t dp::Device::getQueueIndex(const vkb::QueueType queueType) const {
    return getFromVkbResult(device.get_queue_index(queueType));
}

dp::Device::operator vkb::Device() const {
    return device;
}

dp::Device::operator VkDevice() const {
    return device.device;
}
