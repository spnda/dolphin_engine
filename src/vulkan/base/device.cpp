#include "device.hpp"

#include "../utils.hpp"
#include "surface.hpp"

static const std::vector<const char*> deviceExtensions = {
    VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
    VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,

    VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
    VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
    VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
    VK_KHR_SPIRV_1_4_EXTENSION_NAME, // Very important! Our shaders are compiled to SPV1.4 and will not work without.
    VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME,
};

dp::Device::Device(const vkb::Instance& instance, Surface &surface) {
    this->physicalDevice = getPhysicalDevice(instance, surface.surface);
    this->device = getLogicalDevice(instance, this->physicalDevice);
}

vkb::PhysicalDevice dp::Device::getPhysicalDevice(const vkb::Instance& instance, const VkSurfaceKHR& surface) {
    // Get the physical device.
    vkb::PhysicalDeviceSelector physicalDeviceSelector(instance);
    
    // Add the required extensions.
    for (auto ext : deviceExtensions)
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

vkb::Device dp::Device::getLogicalDevice(const vkb::Instance& instance, const vkb::PhysicalDevice& physicalDevice) {
    // Get the logical device.
    vkb::DeviceBuilder deviceBuilder(physicalDevice);
    return getFromVkbResult(deviceBuilder.build());
}

VkCommandPool dp::Device::createDefaultCommandPool(const vkb::Device& device, const uint32_t queueFamilyIndex, const VkCommandPoolCreateFlags flags) {
    VkCommandPoolCreateInfo comandPoolInfo = {};
    comandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    comandPoolInfo.queueFamilyIndex = queueFamilyIndex;
    comandPoolInfo.flags = flags;
    VkCommandPool cmdPool;
    vkCreateCommandPool(device, &comandPoolInfo, nullptr, &cmdPool);
    return cmdPool;
}

vkb::Device& dp::Device::getHandle() {
    return this->device;
}
