#include "HDRIRenderTarget.h"

HDRIRenderTarget::HDRIRenderTarget(vkc::Context& context)
	: m_Images{
		[&context]()-> std::array<vkc::Image, 2>
		{
			vkc::ImageBuilder builder(context);
			builder
				.SetAspectFlags(VK_IMAGE_ASPECT_COLOR_BIT)
				.SetExtent(context.Swapchain.extent)
				.SetFormat(VK_FORMAT_R32G32B32A32_SFLOAT)
				.SetType(VK_IMAGE_TYPE_2D)
				.SetTiling(VK_IMAGE_TILING_OPTIMAL);
			return {
				builder.Build(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, false)
				, builder.Build(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, false)
			};
		}()
	}
	, m_ImageViews
	{
		[&context](std::span<vkc::Image> images)-> std::array<vkc::ImageView, 2>
		{
			return {
				images[0].CreateView(context
									 , VK_IMAGE_VIEW_TYPE_2D
									 , 0
									 , 1
									 , 0
									 , 1
									 , false)
				, images[1].CreateView(context
									   , VK_IMAGE_VIEW_TYPE_2D
									   , 0
									   , 1
									   , 0
									   , 1
									   , false)
			};
		}(m_Images)
	} {}

vkc::ImageView& HDRIRenderTarget::AcquireNextImageView()
{
	++m_ImageIndex;
	m_ImageIndex %= 2;
	return m_ImageViews[m_ImageIndex];
}

void HDRIRenderTarget::Destroy(vkc::Context const& context) const
{
	for (auto& view: m_ImageViews)
		view.Destroy(context);
	for (auto& image: m_Images)
		image.Destroy(context);
}
