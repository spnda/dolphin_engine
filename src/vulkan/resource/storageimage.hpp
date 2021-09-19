#pragma once

#include "../context.hpp"
#include "image.hpp"

namespace dp {
    /** A helper class to easily create and use a storage image for raytracing */
    class StorageImage {
        static const VkFormat imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
        static const uint32_t imageUsage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
        const VkImageSubresourceRange subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

        VkImageLayout currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        const dp::Context& ctx;

    public:
        dp::Image image;

        StorageImage(const dp::Context& context);

        operator dp::Image() const;
        operator VkImage() const;

        VkDescriptorImageInfo getDescriptorImageInfo() const;
        const VkImageLayout getCurrentLayout() const;

        void recreateImage();
        void changeLayout(const VkCommandBuffer commandBuffer, const VkImageLayout newLayout);
    };
}
