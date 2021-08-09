#pragma once

#include <iostream>

#include "VkBootstrap.h"

template<class T>
T getFromVkbResult(vkb::detail::Result<T> result) {
    if (!result) {
        std::cout << result.error().message() << std::endl;
        throw std::runtime_error(result.error().message());
    }
    return result.value();
}

inline bool isFlagSet(const uint32_t val, const uint32_t flag) {
    return (val & flag) == flag;
}
