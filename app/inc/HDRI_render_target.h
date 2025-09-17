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

	[[nodiscard]] std::pair<vkc::Image*, vkc::ImageView*> AcquireNextTarget();

	[[nodiscard]] std::pair<vkc::Image*, vkc::ImageView*> AcquireCurrentTarget();

	[[nodiscard]] std::pair<vkc::Image*, vkc::ImageView*> AcquireLastRenderedToTarget();

	[[nodiscard]] uint32_t GetCurrentImageIndex() const
	{
		assert(HasBeenRenderedTo());
		return static_cast<uint32_t>(m_ImageIndex);
	}

	[[nodiscard]] std::span<vkc::ImageView> GetViews()
	{
		return m_ImageViews;
	}

	[[nodiscard]] std::span<vkc::Image> GetImages()
	{
		return m_Images;
	}

	[[nodiscard]] bool HasBeenRenderedTo() const
	{
		return m_ImageIndex >= 0;
	}

	[[nodiscard]] VkFormat GetFormat() const
	{
		return m_Images[0].GetFormat();
	}

	void Destroy(vkc::Context const& context) const;

private:
	std::array<vkc::Image, 2>     m_Images;
	std::array<vkc::ImageView, 2> m_ImageViews;

	int m_ImageIndex{ -1 };
};

#endif //VULKANRESEARCH_HDRIRENDERTARGET_H
