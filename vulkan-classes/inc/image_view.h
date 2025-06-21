#ifndef IMAGE_VIEW_H
#define IMAGE_VIEW_H

class ImageView final
{
public:
	~ImageView() = default;

	ImageView(ImageView&&)                 = default;
	ImageView(ImageView const&)            = delete;
	ImageView& operator=(ImageView&&)      = default;
	ImageView& operator=(ImageView const&) = delete;

	static void ConvertFromSwapchainVkImageViews(Context& context, std::vector<ImageView>& convertedViews)
	{
		convertedViews.reserve(context.Swapchain.image_count);
		auto const resultImages = context.Swapchain.get_image_views();
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

	operator VkImageView() const
	{
		return m_ImageView;
	}

	operator VkImageView*()
	{
		return &m_ImageView;
	}

private:
	friend class Image;
	ImageView() = default;

	VkImageView m_ImageView{};
};

#endif //IMAGE_VIEW_H
