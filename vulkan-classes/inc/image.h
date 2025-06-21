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
		Context&   context, VkImageViewType type, uint32_t baseLayer = 0, uint32_t layerCount = 1, uint32_t baseMipLevel = 0
		, uint32_t levelCount                                        = 1
	) const;

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

	[[nodiscard]] VkImageLayout GetLayout() const
	{
		return m_Layout;
	}

	[[nodiscard]] VmaAllocation GetAllocation() const
	{
		return m_Allocation;
	}

private:
	friend class ImageBuilder;
	Image() = default;
	VkImage            m_Image{};
	VkImageLayout      m_Layout{};
	VmaAllocation      m_Allocation{};
	VkExtent2D         m_Extent{};
	VkFormat           m_Format{};
	VkImageAspectFlags m_AspectFlags{};
	uint32_t           m_Layers{};
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

	std::string m_FileName;
};

#endif //IMAGE_H
