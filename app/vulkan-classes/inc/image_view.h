#ifndef IMAGE_VIEW_H
#define IMAGE_VIEW_H
#include "context.h"

namespace vkc
{
	class ImageView final
	{
	public:
		~ImageView() = default;

		ImageView(ImageView&&)                 = default;
		ImageView(ImageView const&)            = delete;
		ImageView& operator=(ImageView&&)      = default;
		ImageView& operator=(ImageView const&) = delete;

		static void ConvertFromSwapchainVkImageViews(Context& context, std::vector<ImageView>& convertedViews);

		[[nodiscard]] uint32_t GetBaseLayer() const
		{
			return m_BaseLayer;
		}

		[[nodiscard]] uint32_t GetLayerCount() const
		{
			return m_LayerCount;
		}

		[[nodiscard]] uint32_t GetBaseMipLevel() const
		{
			return m_BaseMipLevel;
		}

		[[nodiscard]] uint32_t GetMipLevelCount() const
		{
			return m_MipLevelCount;
		}

		[[nodiscard]] uint32_t GetType() const
		{
			return m_Type;
		}

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

		VkImageView     m_ImageView{};
		VkImageViewType m_Type{};
		uint32_t        m_BaseLayer{};
		uint32_t        m_BaseMipLevel{};
		uint32_t        m_LayerCount{};
		uint32_t        m_MipLevelCount{};
	};
}

#endif //IMAGE_VIEW_H
