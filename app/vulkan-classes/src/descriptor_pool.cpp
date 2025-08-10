#include "descriptor_pool.h"

DescriptorPoolBuilder& DescriptorPoolBuilder::AddPoolSize(VkDescriptorType type, uint32_t count)
{
	m_DescriptorPoolSizes.emplace_back(type, count);
	return *this;
}

DescriptorPool DescriptorPoolBuilder::Build(uint32_t maxSets, bool addToQueue)
{
	DescriptorPool pool{};

	VkDescriptorPoolCreateInfo poolCreateInfo{};
	poolCreateInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCreateInfo.maxSets       = maxSets;
	poolCreateInfo.poolSizeCount = static_cast<uint32_t>(m_DescriptorPoolSizes.size());
	poolCreateInfo.pPoolSizes    = m_DescriptorPoolSizes.data();

	if (auto const result = m_Context.DispatchTable.createDescriptorPool(&poolCreateInfo, nullptr, pool);
		result != VK_SUCCESS)
		throw std::runtime_error("Failed to create descriptor pool " + result);

	if (addToQueue)
		m_Context.DeletionQueue.Push([context = &m_Context, pool = pool.m_DescPool]
		{
			context->DispatchTable.destroyDescriptorPool(pool, nullptr);
		});
	m_DescriptorPoolSizes.clear();
	return pool;
}
