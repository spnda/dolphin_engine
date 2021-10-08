#pragma once

#include <string>

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include "../context.hpp"

namespace dp {
    class Image {
        const Context& context;

        VmaAllocation allocation = nullptr;

    public:
        VkImage image = nullptr;
        VkImageView imageView = nullptr;

        VkExtent2D imageExtent;

        Image(const Context& context, VkExtent2D extent, VkFormat format, VkImageUsageFlags usageFlags, VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED);

        explicit operator VkImage() const;
        Image& operator=(const dp::Image& newImage);

        /** Destroys the image view, frees all memory and destroys the image. */
        void destroy();
        /** Destroys the image view and frees all memory, but does not destroy the image. */
        void free();

        void setName(const std::string& name);

        static void changeLayout(
            VkImage image,
            VkCommandBuffer commandBuffer,
            VkImageLayout oldLayout,
            VkImageLayout newLayout,
            VkAccessFlags srcAccessMask,
            VkAccessFlags dstAccessMask,
            VkImageSubresourceRange subresourceRange);
    };
}
