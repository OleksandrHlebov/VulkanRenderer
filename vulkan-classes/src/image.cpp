#include "image.h"

ImageView Image::CreateView
(Context& context, VkImageViewType type, uint32_t baseLayer, uint32_t layerCount, uint32_t baseMipLevel, uint32_t levelCount) const
{
	ImageView imageView{};

	VkImageViewCreateInfo imageViewCreateInfo{};
	imageViewCreateInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewCreateInfo.flags                           = 0;
	imageViewCreateInfo.image                           = m_Image;
	imageViewCreateInfo.viewType                        = type;
	imageViewCreateInfo.format                          = m_Format;
	imageViewCreateInfo.subresourceRange.aspectMask     = m_AspectFlags;
	imageViewCreateInfo.subresourceRange.layerCount     = layerCount;
	imageViewCreateInfo.subresourceRange.levelCount     = levelCount;
	imageViewCreateInfo.subresourceRange.baseMipLevel   = baseMipLevel;
	imageViewCreateInfo.subresourceRange.baseArrayLayer = baseLayer;

	if (auto const result = context.DispatchTable.createImageView(&imageViewCreateInfo, nullptr, imageView);
		result != VK_SUCCESS)
		throw std::runtime_error("Failed to create image view");
	context.DeletionQueue.Push([context = &context, imageView = imageView.m_ImageView]
	{
		context->DispatchTable.destroyImageView(imageView, nullptr);
	});

	return imageView;
}

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

	m_Context.DeletionQueue.Push([context = &m_Context, image = image.m_Image, allocation = image.m_Allocation]
	{
		vmaDestroyImage(context->Allocator, image, allocation);
	});

	return image;
}
