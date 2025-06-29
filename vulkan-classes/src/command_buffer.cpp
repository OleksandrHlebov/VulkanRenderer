#include "command_buffer.h"

CommandBuffer::Status CommandBuffer::GetStatus(Context const& context)
{
	if (m_Status != Status::Submitted)
		return m_Status;
	if (context.DispatchTable.getFenceStatus(m_AssociatedFence != VK_NULL_HANDLE
											 ? m_AssociatedFence
											 : m_Fence) == VK_SUCCESS)
		m_Status = Status::Ready;
	return m_Status;
}

VkFence const& CommandBuffer::GetFence() const
{
	return m_Fence;
}

void CommandBuffer::Begin(Context const& context, VkCommandBufferUsageFlags usage)
{
	assert(m_Status == Status::Ready);
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = usage;
	context.DispatchTable.beginCommandBuffer(*this, &beginInfo);
	m_Status = Status::Recording;
}

void CommandBuffer::End(Context const& context)
{
	assert(m_Status == Status::Recording);
	context.DispatchTable.endCommandBuffer(*this);
	m_Status = Status::Executable;
}

void CommandBuffer::Submit
(
	Context const&                     context
	, VkQueue                          queue
	, std::span<VkSemaphoreSubmitInfo> waitSemaphoresInfo
	, std::span<VkSemaphoreSubmitInfo> signalSemaphoresInfo
	, VkFence                          waitFence
)
{
	assert(m_Status == Status::Executable);
	m_AssociatedFence = waitFence;

	if (m_AssociatedFence == VK_NULL_HANDLE)
		if (auto const result = context.DispatchTable.resetFences(1, &m_Fence);
			result != VK_SUCCESS)
			throw std::runtime_error("Failed to reset command buffer internal fence");

	VkCommandBufferSubmitInfo commandBufferSubmitInfo{};
	commandBufferSubmitInfo.sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	commandBufferSubmitInfo.commandBuffer = *this;

	VkSubmitInfo2 submitInfo{};
	submitInfo.sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	submitInfo.commandBufferInfoCount   = 1;
	submitInfo.pCommandBufferInfos      = &commandBufferSubmitInfo;
	submitInfo.waitSemaphoreInfoCount   = static_cast<uint32_t>(waitSemaphoresInfo.size());
	submitInfo.pWaitSemaphoreInfos      = waitSemaphoresInfo.data();
	submitInfo.signalSemaphoreInfoCount = static_cast<uint32_t>(signalSemaphoresInfo.size());
	submitInfo.pSignalSemaphoreInfos    = signalSemaphoresInfo.data();

	if (context.DispatchTable.queueSubmit2(queue
										   , 1
										   , &submitInfo
										   , m_AssociatedFence != VK_NULL_HANDLE
											 ? m_AssociatedFence
											 : m_Fence) != VK_SUCCESS)
		throw std::runtime_error("failed to submit command buffer");

	m_Status = Status::Submitted;
}

CommandBuffer::CommandBuffer(Context& context, VkCommandPool commandPool, VkCommandBuffer buffer)
	: m_Pool{ commandPool }
	, m_CommandBuffer{ buffer }
{
	VkFenceCreateInfo fenceCreateInfo{};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	if (auto const result = context.DispatchTable.createFence(&fenceCreateInfo, nullptr, &m_Fence);
		result != VK_SUCCESS)
		throw std::runtime_error("failed to create fence");

	context.DeletionQueue.Push([context = &context, fence = m_Fence]
	{
		context->DispatchTable.destroyFence(fence, nullptr);
	});
}
