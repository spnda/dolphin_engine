#pragma once

#include <string>
#include <set>

#include <vulkan/vulkan.h>

#include "VkBootstrap.h"

namespace dp {
    // fwd
    class Window;

    class Instance {
        std::set<const char*> requiredExtensions = {};
        vkb::Instance instance = {};

    public:
        explicit Instance() = default;
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
