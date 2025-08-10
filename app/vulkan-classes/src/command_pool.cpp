#include "command_pool.h"

vkc::CommandPool::CommandPool(Context& context, uint32_t queueIndex, uint32_t bufferCount, VkCommandPoolCreateFlags flags)
{
	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags            = flags;
	poolInfo.queueFamilyIndex = queueIndex;
	if (context.DispatchTable.createCommandPool(&poolInfo, nullptr, &m_Pool) != VK_SUCCESS)
		throw std::runtime_error("Failed to create a command pool");

	VkCommandBufferAllocateInfo cmdBufferAllocateInfo{};
	cmdBufferAllocateInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdBufferAllocateInfo.commandBufferCount = bufferCount;
	cmdBufferAllocateInfo.commandPool        = m_Pool;
	cmdBufferAllocateInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

	std::vector<VkCommandBuffer> commandBuffers(bufferCount);

	if (context.DispatchTable.allocateCommandBuffers(&cmdBufferAllocateInfo, commandBuffers.data()) != VK_SUCCESS)
		throw std::runtime_error("Failed to allocate command buffers");

	for (VkCommandBuffer const commandBuffer: commandBuffers)
		m_CommandBuffers.emplace_back(CommandBuffer{ context, *this, commandBuffer });

	context.DeletionQueue.Push([context = &context, pool = m_Pool, commandBuffers]
	{
		context->DispatchTable.freeCommandBuffers(pool
												  , static_cast<uint32_t>(commandBuffers.size())
												  , commandBuffers.data());
		context->DispatchTable.destroyCommandPool(pool, nullptr);
	});
}

vkc::CommandBuffer& vkc::CommandPool::AllocateCommandBuffer(Context& context)
{
	CommandBuffer* foundBuffer{};

	// reset CPU status of all buffers and find any available
	for (CommandBuffer& buffer: m_CommandBuffers)
		if (buffer.GetStatus(context) == CommandBuffer::Status::Ready)
			foundBuffer = &buffer;

	if (foundBuffer)
		return *foundBuffer;

	VkCommandBufferAllocateInfo cmdBufferAllocateInfo{};
	cmdBufferAllocateInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdBufferAllocateInfo.commandBufferCount = 1;
	cmdBufferAllocateInfo.commandPool        = m_Pool;
	cmdBufferAllocateInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

	VkCommandBuffer commandBuffer{};
	if (context.DispatchTable.allocateCommandBuffers(&cmdBufferAllocateInfo, &commandBuffer) != VK_SUCCESS)
		throw std::runtime_error("Failed to allocate command buffers");
	m_CommandBuffers.emplace_back(CommandBuffer{ context, *this, commandBuffer });

	context.DeletionQueue.Push([context = &context, pool = m_Pool, commandBuffer]
	{
		context->DispatchTable.freeCommandBuffers(pool
												  , 1
												  , &commandBuffer);
	});

	return m_CommandBuffers.back();
}

void vkc::CommandPool::Destroy(Context const& context) const
{
	context.DispatchTable.destroyCommandPool(m_Pool, nullptr);
}
