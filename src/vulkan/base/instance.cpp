#include "instance.hpp"

namespace dp {

VulkanInstance::VulkanInstance() {
    window = new Window("Back at it again!", 1920, 1080);
    this->vkInstance = buildInstance("DolphinEngine", VK_MAKE_VERSION(1, 0, 0), requiredExtensions);
}

vkb::Instance VulkanInstance::buildInstance(std::string name, int version, std::vector<const char*> extensions) {
    vkb::InstanceBuilder instance_builder;
    instance_builder.set_app_name(name.c_str());
    instance_builder.set_app_version(version >> 22, version >> 12, version);
    instance_builder.require_api_version(1, 2, 0);
    instance_builder.enable_layer("VK_LAYER_LUNARG_monitor");
    // instance_builder.enable_layer("VK_LAYER_LUNARG_assistant_layer");

    // Add all available extensions
    auto sysInfo = vkb::SystemInfo::get_system_info().value();
    for (auto ext : extensions) {
        if (!sysInfo.is_extension_available(ext)) {
            printf("%s is not available!\n", ext);
            continue;
        }
        instance_builder.enable_extension(ext);
    }

    // Build the instance
    auto buildResult = instance_builder
        .request_validation_layers()
        .use_default_debug_messenger()
        .build();
    return getFromVkbResult(buildResult);
}

} // namespace dp
