#ifndef BUFFER_H
#define BUFFER_H

#include "command_buffer.h"
#include "context.h"
#include "image.h"

template<typename T>
concept IsContainer = requires(T t) { t.data(); t.size(); };

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
		if constexpr (!IsContainer<DataType>)
		{
			memcpy(m_Data, &data, sizeof(DataType));
		}
		else
		{
			memcpy(m_Data, data.data(), data.size() * sizeof(data[0]));
		}
	}

	void CopyTo(Context const& context, CommandBuffer const& commandBuffer, Buffer const& dst) const;

	void CopyTo(Context const& context, CommandBuffer const& commandBuffer, Image const& dst) const;

	void Destroy(Context const& context) const;

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

	BufferBuilder& MapMemory(bool map = true);

	[[nodiscard]] Buffer Build(VkBufferUsageFlags usage, VkDeviceSize size, bool addToQueue = true);

private:
	Context&                m_Context;
	VkBufferCreateInfo      m_BufferCreateInfo{};
	VmaAllocationCreateInfo m_AllocationCreateInfo{};
	bool                    m_MapMemory{ false };
};

#endif //BUFFER_H
