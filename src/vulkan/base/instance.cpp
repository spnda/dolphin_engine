#include "instance.hpp"

#include "../utils.hpp"

dp::Instance::Instance(const dp::Context& context)
    : ctx(context)
#ifdef WITH_NV_AFTERMATH
    , crashTracker(ctx)
#endif // #ifdef WITH_NV_AFTERMATH
    {

}

void dp::Instance::create(const std::string& name) {
#ifdef WITH_NV_AFTERMATH
    // Has to be called *before* crating the "Vulkan device".
    // To be 100% sure this works, we're calling it before creating the VkInstance.
    crashTracker.enable();
#endif // #ifdef WITH_NV_AFTERMATH

    vkb::InstanceBuilder instance_builder;
    instance_builder.set_app_name(name.c_str());
    instance_builder.set_app_version(ctx.appVersion);
    instance_builder.require_api_version(ctx.apiVersion);
#ifdef _DEBUG
    instance_builder.enable_layer("VK_LAYER_LUNARG_monitor");
#endif

    // Add all available extensions
    auto sysInfo = vkb::SystemInfo::get_system_info().value();
    for (auto ext : requiredExtensions) {
        if (!sysInfo.is_extension_available(ext)) {
            fmt::print(stderr, "{} is not available!\n", ext);
            continue;
        }
        instance_builder.enable_extension(ext);
    }

    // Build the instance
    auto buildResult = instance_builder
#ifdef _DEBUG
        .request_validation_layers()
        .use_default_debug_messenger()
#endif
        .build();

    instance = getFromVkbResult(buildResult);
}

void dp::Instance::destroy() const {
#ifdef WITH_NV_AFTERMATH
    crashTracker.disable();
#endif

    vkb::destroy_instance(instance);
}

void dp::Instance::addExtensions(const std::vector<const char*>& extensions) {
    for (auto ext : extensions) {
        requiredExtensions.insert(ext);
    }
}

dp::Instance::operator vkb::Instance() const {
    return instance;
}

dp::Instance::operator VkInstance() const {
    return instance.instance;
}
