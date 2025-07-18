#include "image.h"

ImageView Image::CreateView
(
	Context& context, VkImageViewType type, uint32_t baseLayer, uint32_t layerCount, uint32_t baseMipLevel, uint32_t levelCount, bool
	addToQueue
) const
{
	ImageView imageView{};

	VkImageViewCreateInfo imageViewCreateInfo{};
	imageViewCreateInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewCreateInfo.flags                           = 0;
	imageViewCreateInfo.image                           = *this;
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

	if (addToQueue)
		context.DeletionQueue.Push([context = &context, imageView = imageView.m_ImageView]
		{
			context->DispatchTable.destroyImageView(imageView, nullptr);
		});

	return imageView;
}

void Image::Destroy(Context const& context) const
{
	vmaDestroyImage(context.Allocator, *this, m_Allocation);
}

void Image::MakeTransition(Context const& context, VkCommandBuffer commandBuffer, Transition const& transition)
{
	VkImageMemoryBarrier2 memoryBarrier{};
	memoryBarrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	memoryBarrier.image               = *this;
	memoryBarrier.srcQueueFamilyIndex = transition.SrcQueue;
	memoryBarrier.dstQueueFamilyIndex = transition.DstQueue;
	memoryBarrier.srcAccessMask       = transition.SrcAccessMask;
	memoryBarrier.dstAccessMask       = transition.DstAccessMask;
	memoryBarrier.srcStageMask        = transition.SrcStageMask;
	memoryBarrier.dstStageMask        = transition.DstStageMask;
	memoryBarrier.oldLayout           = m_Layouts[transition.BaseMipLevel][transition.BaseLayer];
	memoryBarrier.newLayout           = transition.NewLayout;

	memoryBarrier.subresourceRange.aspectMask     = m_AspectFlags;
	memoryBarrier.subresourceRange.layerCount     = transition.LayerCount;
	memoryBarrier.subresourceRange.baseArrayLayer = transition.BaseLayer;
	memoryBarrier.subresourceRange.levelCount     = transition.LevelCount;
	memoryBarrier.subresourceRange.baseMipLevel   = transition.BaseMipLevel;

	VkDependencyInfo dependencyInfo{};
	dependencyInfo.sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	dependencyInfo.pNext                   = nullptr;
	dependencyInfo.imageMemoryBarrierCount = 1;
	dependencyInfo.pImageMemoryBarriers    = &memoryBarrier;
	context.DispatchTable.cmdPipelineBarrier2(commandBuffer, &dependencyInfo);

	for (uint32_t mipLevel{}; mipLevel < transition.LevelCount; ++mipLevel)
		for (uint32_t layer{}; layer < transition.LayerCount; ++layer)
			m_Layouts[transition.BaseMipLevel + mipLevel][transition.BaseLayer + layer] = transition.NewLayout;
}

void Image::ConvertFromSwapchainVkImages(Context& context, std::vector<Image>& convertedImages)
{
	convertedImages.clear();
	convertedImages.reserve(context.Swapchain.image_count);
	auto const resultImages = context.Swapchain.get_images();
	if (!resultImages)
		throw std::runtime_error("Failed to get swapchain images " + resultImages.error().message());
	for (VkImage const& image: resultImages.value())
	{
		Image convertedImage{};
		convertedImage.m_Image       = image;
		convertedImage.m_Extent      = context.Swapchain.extent;
		convertedImage.m_Format      = context.Swapchain.image_format;
		convertedImage.m_AspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
		convertedImage.m_MipLevels   = 1;
		convertedImage.m_Layers      = 1;
		convertedImage.m_Layouts.resize(1);
		convertedImage.m_Layouts[0].resize(1);
		convertedImages.emplace_back(std::move(convertedImage));
	}
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

ImageBuilder& ImageBuilder::SetMipLevels(uint32_t mipLevels)
{
	m_MipLevels = mipLevels;
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

ImageBuilder& ImageBuilder::SetSharingMode(VkSharingMode sharingMode)
{
	m_SharingMode = sharingMode;
	return *this;
}

Image ImageBuilder::Build(VkImageUsageFlags usage, bool addToQueue) const
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
	createInfo.sharingMode   = m_SharingMode;
	createInfo.samples       = VK_SAMPLE_COUNT_1_BIT;

	VmaAllocationCreateInfo vmaAllocationCreateInfo{};
	vmaAllocationCreateInfo.flags = 0;
	vmaAllocationCreateInfo.usage = m_MemoryUsage;

	Image image{};
	image.m_AspectFlags = m_AspectFlags;
	image.m_Extent      = m_Extent;
	image.m_Format      = m_Format;
	image.m_Layers      = m_Layers;
	image.m_MipLevels   = m_MipLevels;
	image.m_Layouts.resize(m_MipLevels);
	for (auto& layers: image.m_Layouts)
		layers.resize(m_Layers, VK_IMAGE_LAYOUT_UNDEFINED);

	vmaCreateImage(m_Context.Allocator, &createInfo, &vmaAllocationCreateInfo, image, &image.m_Allocation, nullptr);

	if (addToQueue)
		m_Context.DeletionQueue.Push([context = &m_Context, image = image.m_Image, allocation = image.m_Allocation]
		{
			vmaDestroyImage(context->Allocator, image, allocation);
		});
	return image;
}
