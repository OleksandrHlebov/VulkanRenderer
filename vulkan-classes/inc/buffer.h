#ifndef BUFFER_H
#define BUFFER_H

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

	[[nodiscard]] VkDeviceSize GetSize() const
	{
		return m_AllocationInfo.size;
	}

	template<typename DataType>
	void UpdateData(DataType const& data)
	{
		assert(m_Data);
		memcpy(m_Data, &data, sizeof(DataType));
	}

	operator VkBuffer() const
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
	VkBuffer          m_Buffer{ VK_NULL_HANDLE };
	VmaAllocation     m_Allocation{ VK_NULL_HANDLE };
	VmaAllocationInfo m_AllocationInfo{};
	void*             m_Data{ nullptr };
};

class BufferBuilder final
{
public:
	BufferBuilder() = delete;

	BufferBuilder(Context& context);

	~BufferBuilder() = default;

	BufferBuilder(BufferBuilder&&)                 = delete;
	BufferBuilder(BufferBuilder const&)            = delete;
	BufferBuilder& operator=(BufferBuilder&&)      = delete;
	BufferBuilder& operator=(BufferBuilder const&) = delete;

	BufferBuilder& SetSharingMode(VkSharingMode sharingMode);

	BufferBuilder& SetMemoryUsage(VmaMemoryUsage memoryUsage);

	BufferBuilder& SetRequiredMemoryFlags(VkMemoryPropertyFlags flags);

	Buffer Build(VkBufferUsageFlags usage, VkDeviceSize size, bool mapMemory = false);

private:
	Context&                m_Context;
	VkBufferCreateInfo      m_BufferCreateInfo{};
	VmaAllocationCreateInfo m_AllocationCreateInfo{};
};

#endif //BUFFER_H
