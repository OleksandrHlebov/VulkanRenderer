#ifndef VULKANRESEARCH_HDRIRENDERTARGET_H
#define VULKANRESEARCH_HDRIRENDERTARGET_H

#include <array>

#include "Image.h"

class HDRIRenderTarget
{
public:
	HDRIRenderTarget() = delete;
	HDRIRenderTarget(vkc::Context& context);
	~HDRIRenderTarget() = default;

	HDRIRenderTarget(HDRIRenderTarget&&)                 = delete;
	HDRIRenderTarget(HDRIRenderTarget const&)            = delete;
	HDRIRenderTarget& operator=(HDRIRenderTarget&&)      = delete;
	HDRIRenderTarget& operator=(HDRIRenderTarget const&) = delete;

	vkc::ImageView& AcquireNextImageView();

	[[nodiscard]] bool HasBeenRenderedTo() const
	{
		return m_ImageIndex >= 0;
	}

	void Destroy(vkc::Context const& context) const;

private:
	std::array<vkc::Image, 2>     m_Images;
	std::array<vkc::ImageView, 2> m_ImageViews;

	int m_ImageIndex{ -1 };
};

#endif //VULKANRESEARCH_HDRIRENDERTARGET_H
