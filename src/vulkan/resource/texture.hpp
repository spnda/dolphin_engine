#pragma once

#include <vulkan/vulkan.h>

#include "image.hpp"

namespace dp {
    class Context;

    class Texture {
        static const VkFormat imageFormat = VK_FORMAT_R8G8B8A8_SRGB; // This is the format STB uses.
        const VkImageSubresourceRange subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

        const dp::Context& ctx;
        dp::Image image;
        std::string name;
        VkSampler sampler = nullptr;

    public:
        Texture(const dp::Context& context, VkExtent2D imageSize, std::string name = "texture");

        operator dp::Image() const;
        operator VkImage() const;
        explicit operator VkImageView() const;

        void create();
        void changeLayout(VkCommandBuffer commandBuffer, VkImageLayout newLayout);

        [[nodiscard]] VkSampler getSampler() const;
        [[nodiscard]] VkExtent2D getSize() const;
        [[nodiscard]] VkImageLayout getCurrentLayout() const;
    };
}
