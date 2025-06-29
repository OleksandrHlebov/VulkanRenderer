
#ifndef COMMAND_BUFFER_H
#define COMMAND_BUFFER_H
#include <span>

#include "context.h"

class CommandBuffer
{
public:
	enum class Status
	{
		Recording, Executable, Submitted, Ready
	};

	CommandBuffer()  = delete;
	~CommandBuffer() = default;

	CommandBuffer(CommandBuffer&&)                 = default;
	CommandBuffer(CommandBuffer const&)            = delete;
	CommandBuffer& operator=(CommandBuffer&&)      = default;
	CommandBuffer& operator=(CommandBuffer const&) = delete;

	[[nodiscard]] Status GetStatus(Context const& context);

	[[nodiscard]] VkFence const& GetFence() const;

	void Begin(Context const& context, VkCommandBufferUsageFlags usage = 0);

	void End(Context const& context);

	void Submit
	(
		Context const&                     context
		, VkQueue                          queue
		, std::span<VkSemaphoreSubmitInfo> waitSemaphoresInfo
		, std::span<VkSemaphoreSubmitInfo> signalSemaphoresInfo
		, VkFence                          waitFence = VK_NULL_HANDLE
	);

	operator VkCommandBuffer() const
	{
		return m_CommandBuffer;
	}

private:
	friend class CommandPool;

	CommandBuffer(Context& context, VkCommandPool commandPool, VkCommandBuffer buffer);

	VkCommandPool   m_Pool;
	VkCommandBuffer m_CommandBuffer;
	VkFence         m_Fence{};
	VkFence         m_AssociatedFence{};
	Status          m_Status{ Status::Ready };
};

#endif //COMMAND_BUFFER_H
