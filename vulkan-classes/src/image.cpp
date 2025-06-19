#include "image.h"

ImageBuilder& ImageBuilder::SetFormat(VkFormat format)
{
	m_Format = format;
	return *this;
}

ImageBuilder& ImageBuilder::SetTiling(VkImageTiling tiling)
{
	m_Tiling = tiling;
	return *this;
}

ImageBuilder& ImageBuilder::SetAspectFlags(VkImageAspectFlags aspectFlags)
{
	m_AspectFlags = aspectFlags;
	return *this;
}

ImageBuilder& ImageBuilder::SetLayers(uint32_t layers)
{
	m_Layers = layers;
	return *this;
}

ImageBuilder& ImageBuilder::SetType(VkImageType type)
{
	m_ImageType = type;
	return *this;
}

ImageBuilder& ImageBuilder::SetExtent(VkExtent2D extent)
{
	m_Extent = extent;
	return *this;
}

ImageBuilder& ImageBuilder::SetMemoryUsage(VmaMemoryUsage memoryUsage)
{
	m_MemoryUsage = memoryUsage;
	return *this;
}

ImageBuilder& ImageBuilder::SetFlags(VkImageCreateFlags flags)
{
	m_CreationFlags = flags;
	return *this;
}

ImageBuilder& ImageBuilder::SetFileName(std::string const& fileName)
{
	m_FileName = fileName;
	return *this;
}

Image ImageBuilder::Build(VkImageUsageFlags usage) const
{
	VkImageCreateInfo createInfo{};
	createInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	createInfo.pNext         = nullptr;
	createInfo.imageType     = m_ImageType;
	createInfo.format        = m_Format;
	createInfo.tiling        = m_Tiling;
	createInfo.extent.width  = m_Extent.width;
	createInfo.extent.height = m_Extent.height;
	createInfo.extent.depth  = 1;
	createInfo.arrayLayers   = m_Layers;
	createInfo.usage         = usage;
	createInfo.mipLevels     = 1;
	createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	createInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
	createInfo.samples       = VK_SAMPLE_COUNT_1_BIT;

	VmaAllocationCreateInfo vmaAllocationCreateInfo{};
	vmaAllocationCreateInfo.flags = 0;
	vmaAllocationCreateInfo.usage = m_MemoryUsage;

	Image image{};
	image.m_AspectFlags = m_AspectFlags;
	image.m_Extent      = m_Extent;
	image.m_Format      = m_Format;
	image.m_Layers      = m_Layers;
	image.m_Layout      = VK_IMAGE_LAYOUT_UNDEFINED;

	vmaCreateImage(m_Context.Allocator, &createInfo, &vmaAllocationCreateInfo, image, &image.m_Allocation, nullptr);

	return image;
}
