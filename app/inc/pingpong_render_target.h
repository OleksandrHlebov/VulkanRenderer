#ifndef VULKANRESEARCH_PINGPONGRENDERTARGET_H
#define VULKANRESEARCH_PINGPONGRENDERTARGET_H

#include <array>

#include "Image.h"

class PingPongRenderTarget
{
public:
	PingPongRenderTarget() = delete;
	PingPongRenderTarget(vkc::Context& context, VkFormat format);
	~PingPongRenderTarget() = default;

	PingPongRenderTarget(PingPongRenderTarget&&)                 = delete;
	PingPongRenderTarget(PingPongRenderTarget const&)            = delete;
	PingPongRenderTarget& operator=(PingPongRenderTarget&&)      = delete;
	PingPongRenderTarget& operator=(PingPongRenderTarget const&) = delete;

	[[nodiscard]] std::pair<vkc::Image*, vkc::ImageView*> AcquireNextTarget();

	[[nodiscard]] std::pair<vkc::Image*, vkc::ImageView*> AcquireCurrentTarget();

	[[nodiscard]] std::pair<vkc::Image*, vkc::ImageView*> AcquireLastRenderedToTarget();

	[[nodiscard]] uint32_t GetCurrentImageIndex() const
	{
		assert(HasBeenRenderedTo());
		return static_cast<uint32_t>(m_ImageIndex);
	}

	[[nodiscard]] uint32_t GetLastImageIndex() const
	{
		assert(HasBeenRenderedTo());
		return static_cast<uint32_t>(abs(m_ImageIndex - 1) % 2);
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

#endif //VULKANRESEARCH_PINGPONGRENDERTARGET_H
