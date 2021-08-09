#include "device.hpp"

#include "../utils.hpp"

namespace dp {

Device::Device(vkb::Instance &instance, Surface &surface) {
    this->physicalDevice = getPhysicalDevice(instance, surface.surface);
    this->device = getLogicalDevice(instance.instance, this->physicalDevice);
};

/**
 * Get the physical device per vkb::PhysicalDeviceSelector from
 * the given instance and surface.
 */
vkb::PhysicalDevice Device::getPhysicalDevice(vkb::Instance instance, VkSurfaceKHR surface) {
    // Get the physical device.
    vkb::PhysicalDeviceSelector physicalDeviceSelector(instance);
    
    // Add the required extensions.
    for (auto ext : dp::deviceExtensions)
        physicalDeviceSelector.add_required_extension(ext);

    // Should conditionally add these feature, but heck, whos gonna use this besides me.
    {
        VkPhysicalDeviceBufferDeviceAddressFeatures deviceAddressFeatures = {};
        deviceAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
        deviceAddressFeatures.bufferDeviceAddress = true;
        physicalDeviceSelector.add_required_extension_features(deviceAddressFeatures);

        VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures = {};
        rayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
        rayTracingPipelineFeatures.rayTracingPipeline = true;
        physicalDeviceSelector.add_required_extension_features(rayTracingPipelineFeatures);

        VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeature = {};
        accelerationStructureFeature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
        accelerationStructureFeature.accelerationStructure = true;
        physicalDeviceSelector.add_required_extension_features(accelerationStructureFeature);
    }

    // Let vk-bootstrap select our physical device.
    return getFromVkbResult(physicalDeviceSelector.set_surface(surface).select());
}

/**
 * Get the logical device for the given physical device.
 */
vkb::Device Device::getLogicalDevice(VkInstance instance, vkb::PhysicalDevice physicalDevice) {
    // Get the logical device.
    vkb::DeviceBuilder deviceBuilder(physicalDevice);
    return getFromVkbResult(deviceBuilder.build());
}

VkCommandPool Device::createDefaultCommandPool(vkb::Device device, uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags) {
    VkCommandPoolCreateInfo comandPoolInfo = {};
    comandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    comandPoolInfo.queueFamilyIndex = queueFamilyIndex;
    comandPoolInfo.flags = flags;
    VkCommandPool cmdPool;
    vkCreateCommandPool(device.device, &comandPoolInfo, nullptr, &cmdPool);
    return cmdPool;
}

vkb::Device Device::getDevice() {
    return this->device;
}

void Device::waitForIdle() {
    vkDeviceWaitIdle(this->device.device);
}

} // namespace dp
