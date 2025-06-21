#ifndef BUFFER_H
#define BUFFER_H
#include <vulkan/vulkan_core.h>

#include "context.h"
#include "vma_usage.h"

class Buffer final
{
public:
	~Buffer() = default;

	Buffer(Buffer&&)                 = default;
	Buffer(Buffer const&)            = delete;
	Buffer& operator=(Buffer&&)      = default;
	Buffer& operator=(Buffer const&) = delete;

	template<typename DataType>
	void UpdateData(DataType const& data)
	{
		assert(m_Data);
		memcpy(m_Data, &data, sizeof(DataType));
	}

	operator VkBuffer&()
	{
		return m_Buffer;
	}

	operator VkBuffer const&() const
	{
		return m_Buffer;
	}

	operator VkBuffer*()
	{
		return &m_Buffer;
	}

	operator VkBuffer const*() const
	{
		return &m_Buffer;
	}

private:
	friend class BufferBuilder;
	Buffer() = default;
	VkBuffer      m_Buffer{ VK_NULL_HANDLE };
	VmaAllocation m_Allocation{ VK_NULL_HANDLE };
	void*         m_Data{ nullptr };
};

class BufferBuilder final
{
public:
	BufferBuilder() = delete;

	BufferBuilder(Context& context)
		: m_Context{ context }
	{
		m_BufferCreateInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		m_BufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}

	~BufferBuilder() = default;

	BufferBuilder(BufferBuilder&&)                 = delete;
	BufferBuilder(BufferBuilder const&)            = delete;
	BufferBuilder& operator=(BufferBuilder&&)      = delete;
	BufferBuilder& operator=(BufferBuilder const&) = delete;

	BufferBuilder& SetSharingMode(VkSharingMode sharingMode)
	{
		m_BufferCreateInfo.sharingMode = sharingMode;
		return *this;
	}

	BufferBuilder& SetMemoryUsage(VmaMemoryUsage memoryUsage)
	{
		m_AllocationCreateInfo.usage = memoryUsage;
		return *this;
	}

	BufferBuilder& SetRequiredMemoryFlags(VkMemoryPropertyFlags flags)
	{
		m_AllocationCreateInfo.requiredFlags = flags;
		return *this;
	}

	Buffer Build(VkBufferUsageFlags usage, VkDeviceSize size, bool mapMemory = false)
	{
		Buffer buffer{};

		m_BufferCreateInfo.usage = usage;
		m_BufferCreateInfo.size  = size;

		vmaCreateBuffer(m_Context.Allocator, &m_BufferCreateInfo, &m_AllocationCreateInfo, buffer, &buffer.m_Allocation, nullptr);
		if (mapMemory)
			vmaMapMemory(m_Context.Allocator, buffer.m_Allocation, &buffer.m_Data);

		m_Context.DeletionQueue.Push([context = &m_Context
										 , buffer = buffer.m_Buffer
										 , allocation = buffer.m_Allocation]
									 {
										 vmaUnmapMemory(context->Allocator, allocation);
										 vmaDestroyBuffer(context->Allocator, buffer, allocation);
									 });

		return buffer;
	}

private:
	Context&                m_Context;
	VkBufferCreateInfo      m_BufferCreateInfo{};
	VmaAllocationCreateInfo m_AllocationCreateInfo{};
};

#endif //BUFFER_H
