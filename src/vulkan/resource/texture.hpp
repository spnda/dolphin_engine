#pragma once

#include <vulkan/vulkan.h>

#include "image.hpp"

namespace dp {
    class Context;

    class Texture : public Image {
        static const VkFormat imageFormat = VK_FORMAT_R8G8B8A8_SRGB; // This is the format STB uses.
        const VkImageSubresourceRange subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

        std::string name;
        VkSampler sampler = nullptr;

    public:
        Texture(const dp::Context& context, VkExtent2D imageSize, std::string name = "texture");

        explicit operator VkImageView() const;

        void create();
        void changeLayout(
            VkCommandBuffer commandBuffer,
            VkImageLayout newLayout,
            VkPipelineStageFlags srcStage,
            VkPipelineStageFlags dstStage);

        [[nodiscard]] VkSampler getSampler() const;
    };
}
