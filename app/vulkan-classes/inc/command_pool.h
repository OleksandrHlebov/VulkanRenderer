#ifndef COMMAND_POOL_H
#define COMMAND_POOL_H
#include <list>

#include "command_buffer.h"

namespace vkc
{
	class CommandPool final
	{
	public:
		CommandPool() = delete;

		CommandPool(Context& context, uint32_t queueIndex, uint32_t bufferCount, VkCommandPoolCreateFlags flags = 0);

		~CommandPool() = default;

		CommandPool(CommandPool&&)                 = delete;
		CommandPool(CommandPool const&)            = delete;
		CommandPool& operator=(CommandPool&&)      = delete;
		CommandPool& operator=(CommandPool const&) = delete;

		CommandBuffer& AllocateCommandBuffer(Context& context);

		void Destroy(Context const& context) const;

		operator VkCommandPool*()
		{
			return &m_Pool;
		}

		operator VkCommandPool() const
		{
			return m_Pool;
		}

	private:
		VkCommandPool m_Pool{};

		// list to avoid breaking references, it is read only sequentially everytime anyway
		std::list<CommandBuffer> m_CommandBuffers;
	};
}

#endif //COMMAND_POOL_H
