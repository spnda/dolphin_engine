#include "renderpass.hpp"

#include <array>

dp::RenderPass::RenderPass(const dp::Context& context, const dp::VulkanSwapchain& swapchain)
		: ctx(context), swapchain(swapchain) {

}

void dp::RenderPass::create(const VkAttachmentLoadOp colorBufferLoadOp, const std::string name) {
	VkAttachmentDescription colorAttachment = {
		.format = swapchain.swapchain.image_format,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.loadOp = colorBufferLoadOp,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.initialLayout = colorBufferLoadOp == VK_ATTACHMENT_LOAD_OP_CLEAR ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
	};

	VkAttachmentReference colorAttachmentRef = {
		.attachment = 0,
		.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
	};

	VkSubpassDescription subpass = {
		.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
		.colorAttachmentCount = 1,
		.pColorAttachments = &colorAttachmentRef,
	};

	VkSubpassDependency dependency = {
		.srcSubpass = VK_SUBPASS_EXTERNAL,
		.dstSubpass = 0,
		.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		.srcAccessMask = 0,
		.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
	};

	std::array<VkAttachmentDescription, 1> attachments = { colorAttachment };

	VkRenderPassCreateInfo renderPassInfo = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount = static_cast<uint32_t>(attachments.size()),
		.pAttachments = attachments.data(),
		.subpassCount = 1,
		.pSubpasses = &subpass,
		.dependencyCount = 1,
		.pDependencies = &dependency
	};

	vkCreateRenderPass(ctx.device, &renderPassInfo, nullptr, &handle);

	if (name.size() != 0)
		ctx.setDebugUtilsName(handle, name);
}

void dp::RenderPass::begin(const VkCommandBuffer cmdBuffer, const VkFramebuffer framebuffer, std::vector<VkClearValue> clearValues) {
	VkRenderPassBeginInfo renderPassInfo = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass = handle,
		.framebuffer = framebuffer,
		.renderArea = {
			.offset = { 0, 0 },
			.extent = swapchain.getExtent(),
		},
		.clearValueCount = static_cast<uint32_t>(clearValues.size()),
		.pClearValues = clearValues.data(),
	};

	vkCmdBeginRenderPass(cmdBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void dp::RenderPass::end(const VkCommandBuffer cmdBuffer) {
	vkCmdEndRenderPass(cmdBuffer);
}

dp::RenderPass::operator VkRenderPass() const {
	return handle;
}
