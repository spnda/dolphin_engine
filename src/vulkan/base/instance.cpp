#include "instance.hpp"

#include "../../sdl/window.hpp"
#include "../utils.hpp"

dp::VulkanInstance::VulkanInstance() {
    window = new Window("Back at it again!", 1920, 1080);
    this->vkInstance = buildInstance("DolphinEngine", VK_MAKE_VERSION(1, 0, 0), requiredExtensions);
}

vkb::Instance dp::VulkanInstance::buildInstance(const std::string name, const uint32_t version, const std::vector<const char*> extensions) {
    vkb::InstanceBuilder instance_builder;
    instance_builder.set_app_name(name.c_str());
    instance_builder.set_app_version(version >> 22, version >> 12, version);
    instance_builder.require_api_version(1, 2, 0);
#ifdef NDEBUG
    instance_builder.enable_layer("VK_LAYER_LUNARG_monitor");
#endif

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
// #ifdef NDEBUG
        .request_validation_layers()
        .use_default_debug_messenger()
// #endif
        .build();
    return getFromVkbResult(buildResult);
}
