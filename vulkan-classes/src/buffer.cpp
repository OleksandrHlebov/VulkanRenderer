#include "buffer.h"

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

Buffer BufferBuilder::Build(VkBufferUsageFlags usage, VkDeviceSize size, bool mapMemory)
{
	Buffer buffer{};

	m_BufferCreateInfo.usage = usage;
	m_BufferCreateInfo.size  = size;

	vmaCreateBuffer(m_Context.Allocator, &m_BufferCreateInfo, &m_AllocationCreateInfo, buffer, &buffer.m_Allocation, nullptr);
	if (mapMemory)
		vmaMapMemory(m_Context.Allocator, buffer.m_Allocation, &buffer.m_Data);

	vmaGetAllocationInfo(m_Context.Allocator, buffer.m_Allocation, &buffer.m_AllocationInfo);

	m_Context.DeletionQueue.Push([context = &m_Context
									 , buffer = buffer.m_Buffer
									 , allocation = buffer.m_Allocation]
								 {
									 vmaUnmapMemory(context->Allocator, allocation);
									 vmaDestroyBuffer(context->Allocator, buffer, allocation);
								 });

	return buffer;
}
