#pragma once

#include <string>
#include <set>

#include <vulkan/vulkan.h>

#include "VkBootstrap.h"

#ifdef WITH_NV_AFTERMATH
#include "crash_tracker.hpp"
#endif // #ifdef WITH_NV_AFTERMATH

namespace dp {
    // fwd
    class Context;
    class Window;

    class Instance {
        const dp::Context& ctx;

        std::set<const char*> requiredExtensions = {};
        vkb::Instance instance = {};

#ifdef WITH_NV_AFTERMATH
        dp::GpuCrashTracker crashTracker;
#endif // #ifdef WITH_NV_AFTERMATH

    public:
        explicit Instance(const dp::Context& ctx);
        Instance(const Instance &) = default;
        Instance& operator=(const Instance &) = default;

        void create(const std::string& name);
        void destroy() const;
        void addExtensions(const std::vector<const char*>& extensions);

        template<class T>
        T getFunctionAddress(const std::string& functionName) const {
            return reinterpret_cast<T>(vkGetInstanceProcAddr(instance, functionName.c_str()));
        }

        explicit operator vkb::Instance() const;
        operator VkInstance() const;
    };
} // namespace dp
