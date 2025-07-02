#include "buffer.h"

void Buffer::CopyTo(Context const& context, CommandBuffer const& commandBuffer, Buffer const& dst) const
{
	assert(this->GetSize() == dst.GetSize());

	VkBufferCopy2 copyRegion{};
	copyRegion.sType     = VK_STRUCTURE_TYPE_BUFFER_COPY_2;
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	copyRegion.size      = this->GetSize();

	VkCopyBufferInfo2 info{};
	info.sType       = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2;
	info.srcBuffer   = *this;
	info.dstBuffer   = dst;
	info.regionCount = 1;
	info.pRegions    = &copyRegion;

	context.DispatchTable.cmdCopyBuffer2(commandBuffer, &info);
}

void Buffer::CopyTo(Context const& context, CommandBuffer const& commandBuffer, Image const& dst) const
{
	VkBufferImageCopy2 copyRegion{};
	copyRegion.sType             = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2;
	copyRegion.bufferOffset      = 0;
	copyRegion.bufferRowLength   = 0;
	copyRegion.bufferImageHeight = 0;

	copyRegion.imageExtent                     = VkExtent3D{ dst.GetExtent().width, dst.GetExtent().height, 1 };
	copyRegion.imageOffset                     = VkOffset3D{};
	copyRegion.imageSubresource.aspectMask     = dst.GetAspect();
	copyRegion.imageSubresource.layerCount     = dst.GetLayerCount();
	copyRegion.imageSubresource.mipLevel       = 0;
	copyRegion.imageSubresource.baseArrayLayer = 0;

	VkCopyBufferToImageInfo2 copyImageInfo{};
	copyImageInfo.sType          = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2;
	copyImageInfo.dstImage       = dst;
	copyImageInfo.dstImageLayout = dst.GetLayout();
	copyImageInfo.srcBuffer      = *this;
	copyImageInfo.regionCount    = 1;
	copyImageInfo.pRegions       = &copyRegion;
	context.DispatchTable.cmdCopyBufferToImage2(commandBuffer, &copyImageInfo);
}

void Buffer::Destroy(Context const& context) const
{
	if (m_Data)
		vmaUnmapMemory(context.Allocator, m_Allocation);
	vmaDestroyBuffer(context.Allocator, *this, m_Allocation);
}

BufferBuilder::BufferBuilder(Context& context)
	: m_Context{ context }
{
	m_BufferCreateInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	m_BufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
}

BufferBuilder& BufferBuilder::SetSharingMode(VkSharingMode sharingMode)
{
	m_BufferCreateInfo.sharingMode = sharingMode;
	return *this;
}

BufferBuilder& BufferBuilder::SetMemoryUsage(VmaMemoryUsage memoryUsage)
{
	m_AllocationCreateInfo.usage = memoryUsage;
	return *this;
}

BufferBuilder& BufferBuilder::SetRequiredMemoryFlags(VkMemoryPropertyFlags flags)
{
	m_AllocationCreateInfo.requiredFlags = flags;
	return *this;
}

BufferBuilder& BufferBuilder::MapMemory(bool map)
{
	m_MapMemory = map;
	return *this;
}

Buffer BufferBuilder::Build(VkBufferUsageFlags usage, VkDeviceSize size, bool addToQueue)
{
	Buffer buffer{};

	m_BufferCreateInfo.usage = usage;
	m_BufferCreateInfo.size  = size;

	vmaCreateBuffer(m_Context.Allocator, &m_BufferCreateInfo, &m_AllocationCreateInfo, buffer, &buffer.m_Allocation, nullptr);
	if (m_MapMemory)
		vmaMapMemory(m_Context.Allocator, buffer.m_Allocation, &buffer.m_Data);

	vmaGetAllocationInfo(m_Context.Allocator, buffer.m_Allocation, &buffer.m_AllocationInfo);

	if (addToQueue)
		m_Context.DeletionQueue.Push([context = &m_Context
										 , buffer = buffer.m_Buffer
										 , allocation = buffer.m_Allocation
										 , mapped = m_MapMemory]
									 {
										 if (mapped)
											 vmaUnmapMemory(context->Allocator, allocation);
										 vmaDestroyBuffer(context->Allocator, buffer, allocation);
									 });

	return buffer;
}
