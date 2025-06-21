#ifndef IMAGE_H
#define IMAGE_H
#include "context.h"
#include "image_view.h"
#include "vma_usage.h"

class Image final
{
public:
	~Image() = default;

	Image(Image&&)                 = default;
	Image(Image const&)            = delete;
	Image& operator=(Image&&)      = default;
	Image& operator=(Image const&) = delete;

	ImageView CreateView
	(
		Context&          context
		, VkImageViewType type
		, uint32_t        baseLayer    = 0
		, uint32_t        layerCount   = 1
		, uint32_t        baseMipLevel = 0
		, uint32_t        levelCount   = 1
	) const;

	struct Transition
	{
		VkAccessFlags2       SrcAccessMask{};
		VkAccessFlags2       DstAccessMask{};
		VkPipelineStageFlags SrcStageMask{};
		VkPipelineStageFlags DstStageMask{};
		VkImageLayout        NewLayout{};

		uint32_t BaseLayer{ 0 };
		uint32_t LayerCount{ 1 };
		uint32_t BaseMipLevel{ 0 };
		uint32_t LevelCount{ 1 };

		uint32_t SrcQueue{ VK_QUEUE_FAMILY_IGNORED };
		uint32_t DstQueue{ VK_QUEUE_FAMILY_IGNORED };
	};

	void MakeTransition(Context const& context, VkCommandBuffer commandBuffer, Transition const& transition) const;

	[[nodiscard]] VkImageLayout GetLayout(uint32_t mipLevel, uint32_t layer) const
	{
		assert(mipLevel < m_MipLevels && layer < m_Layers);

		return m_Layouts[mipLevel][layer];
	}

	[[nodiscard]] VmaAllocation GetAllocation() const
	{
		return m_Allocation;
	}

	static void ConvertFromSwapchainVkImages
	(Context& context, std::vector<Image>& convertedImages);

	operator VkImage() const
	{
		return m_Image;
	}

	operator VkImage*()
	{
		return &m_Image;
	}

	operator VkImage const*() const
	{
		return &m_Image;
	}

private:
	friend class ImageBuilder;
	Image() = default;
	using Layers    = std::vector<VkImageLayout>;
	using MipLevels = std::vector<Layers>;
	VkImage            m_Image{};
	MipLevels          m_Layouts{};
	VmaAllocation      m_Allocation{};
	VkExtent2D         m_Extent{};
	VkFormat           m_Format{};
	VkImageAspectFlags m_AspectFlags{};
	uint32_t           m_Layers{};
	uint32_t           m_MipLevels{};
};

class ImageBuilder final
{
public:
	ImageBuilder(Context& context)
		: m_Context{ context } {}

	~ImageBuilder() = default;

	ImageBuilder(ImageBuilder&&)                 = delete;
	ImageBuilder(ImageBuilder const&)            = delete;
	ImageBuilder& operator=(ImageBuilder const&) = delete;
	ImageBuilder& operator=(ImageBuilder&&)      = delete;

	ImageBuilder& SetFormat(VkFormat format);
	ImageBuilder& SetTiling(VkImageTiling tiling);
	ImageBuilder& SetAspectFlags(VkImageAspectFlags aspectFlags);
	ImageBuilder& SetLayers(uint32_t layers);
	ImageBuilder& SetMipLevels(uint32_t mipLevels);
	ImageBuilder& SetType(VkImageType type);
	ImageBuilder& SetExtent(VkExtent2D extent);
	ImageBuilder& SetMemoryUsage(VmaMemoryUsage memoryUsage);
	ImageBuilder& SetFlags(VkImageCreateFlags flags);
	ImageBuilder& SetFileName(std::string const& fileName);

	[[nodiscard]] Image Build(VkImageUsageFlags usage) const;

private:
	Context&           m_Context;
	VkFormat           m_Format{ VK_FORMAT_R8G8B8A8_SRGB };
	VkExtent2D         m_Extent{};
	VkImageTiling      m_Tiling{ VK_IMAGE_TILING_OPTIMAL };
	VkImageUsageFlags  m_Usage{};
	VkImageType        m_ImageType{};
	VkImageCreateFlags m_CreationFlags{};
	VkImageAspectFlags m_AspectFlags{ VK_IMAGE_ASPECT_COLOR_BIT };
	VmaMemoryUsage     m_MemoryUsage{ VMA_MEMORY_USAGE_AUTO };
	uint32_t           m_Layers{ 1 };
	uint32_t           m_MipLevels{ 1 };

	std::string m_FileName;
};

#endif //IMAGE_H
