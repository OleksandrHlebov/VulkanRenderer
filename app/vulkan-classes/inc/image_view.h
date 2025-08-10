#ifndef IMAGE_VIEW_H
#define IMAGE_VIEW_H
#include "context.h"

class ImageView final
{
public:
	~ImageView() = default;

	ImageView(ImageView&&)                 = default;
	ImageView(ImageView const&)            = delete;
	ImageView& operator=(ImageView&&)      = default;
	ImageView& operator=(ImageView const&) = delete;

	static void ConvertFromSwapchainVkImageViews(Context& context, std::vector<ImageView>& convertedViews);

	void Destroy(Context const& context) const;

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
