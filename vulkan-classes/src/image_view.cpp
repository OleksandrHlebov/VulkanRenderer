#include "image_view.h"

void ImageView::ConvertFromSwapchainVkImageViews(Context& context, std::vector<ImageView>& convertedViews)
{
	convertedViews.reserve(context.Swapchain.image_count);
	auto resultImages = context.Swapchain.get_image_views();
	if (!resultImages)
		throw std::runtime_error("Failed to get swapchain images " + resultImages.error().message());
	for (VkImageView const& view: resultImages.value())
	{
		ImageView convertedView{};
		convertedView.m_ImageView = view;
		convertedViews.emplace_back(std::move(convertedView));
	}
	context.DeletionQueue.Push([context = &context, views = std::move(resultImages.value())]
	{
		context->Swapchain.destroy_image_views(views);
	});
}
