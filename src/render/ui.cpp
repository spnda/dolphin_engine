#include "ui.hpp"

#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <imgui_impl_sdl.h>

#include "../sdl/window.hpp"
#include "../vulkan/base/swapchain.hpp"

dp::Ui::Ui(const dp::Context& context, const dp::VulkanSwapchain& vkSwapchain)
        : ctx(context), swapchain(vkSwapchain), renderPass(ctx, swapchain) {

}

void dp::Ui::destroy() {
    renderPass.destroy();

    ImGui_ImplSDL2_Shutdown();
    ImGui_ImplVulkan_Shutdown();
    ImGui::DestroyContext();
}

void dp::Ui::initFramebuffers() {
    auto extent = swapchain.getExtent();
    VkFramebufferCreateInfo frameBufferCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .pNext = nullptr,
        .renderPass = renderPass,
        .attachmentCount = 1,
        .width = extent.width,
        .height = extent.height,
        .layers = 1,
    };

    const uint32_t swapchainImageCount = swapchain.images.size();
    framebuffers = std::vector<VkFramebuffer>(swapchainImageCount);

    for (uint32_t i = 0; i < swapchainImageCount; i++) {
        frameBufferCreateInfo.pAttachments = &swapchain.views[i];
        vkCreateFramebuffer(ctx.device, &frameBufferCreateInfo, nullptr, &framebuffers[i]);
    }
}

void dp::Ui::init() {
    std::vector<VkDescriptorPoolSize> poolSizes = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };

    ctx.createDescriptorPool(1000, poolSizes, &descriptorPool);
    renderPass.create(VK_ATTACHMENT_LOAD_OP_LOAD, "uiPass");

    initFramebuffers();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGui::GetIO().WantCaptureMouse = true;

    if (!ImGui_ImplSDL2_InitForVulkan(ctx.window->window)) {}

    ImGui_ImplVulkan_InitInfo initInfo = {
        .Instance = ctx.instance,
        .PhysicalDevice = ctx.physicalDevice,
        .Device = ctx.device,
        .Queue = ctx.graphicsQueue,
        .DescriptorPool = descriptorPool,
        .MinImageCount = 3,
        .ImageCount = 3,
        .MSAASamples = VK_SAMPLE_COUNT_1_BIT
    };

    ImGui_ImplVulkan_Init(&initInfo, renderPass);

    ImGui::StyleColorsDark();

    auto cmdBuffer = ctx.createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, ctx.commandPool, true);
    ImGui_ImplVulkan_CreateFontsTexture(cmdBuffer);
    ctx.flushCommandBuffer(cmdBuffer, ctx.graphicsQueue);
    ImGui_ImplVulkan_DestroyFontUploadObjects(); // Just CPU data.
}

void dp::Ui::prepare() {
    ImGui_ImplSDL2_NewFrame();
    ImGui_ImplVulkan_NewFrame();
    ImGui::NewFrame();
}

void dp::Ui::draw(const VkCommandBuffer cmdBuffer) {
    ImGui::ShowDemoWindow();

    ImGui::Render();

    static const std::vector<VkClearValue> clearValues = {
        { .color = { 0.0f, 0.0f, 0.2f, 0.0f }, }
    };

    renderPass.begin(cmdBuffer, framebuffers[ctx.currentImageIndex], clearValues);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuffer);
    renderPass.end(cmdBuffer);
}

void dp::Ui::resize() {
    for (const auto& framebuffer : framebuffers) {
        vkDestroyFramebuffer(ctx.device, framebuffer, nullptr);
    }
    initFramebuffers();
}

bool dp::Ui::isInputting() {
    return ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) || ImGui::IsAnyItemActive();
}
