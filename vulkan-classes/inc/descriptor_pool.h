#ifndef DESCRIPTOR_POOL_H
#define DESCRIPTOR_POOL_H
#include "context.h"

class DescriptorPool final
{
public:
	~DescriptorPool() = default;

	DescriptorPool(DescriptorPool&&)                 = default;
	DescriptorPool(DescriptorPool const&)            = delete;
	DescriptorPool& operator=(DescriptorPool&&)      = default;
	DescriptorPool& operator=(DescriptorPool const&) = delete;

	operator VkDescriptorPool() const
	{
		return m_DescPool;
	}

	operator VkDescriptorPool*()
	{
		return &m_DescPool;
	}

private:
	friend class DescriptorPoolBuilder;
	DescriptorPool() = default;

	VkDescriptorPool m_DescPool;
};

class DescriptorPoolBuilder final
{
public:
	DescriptorPoolBuilder() = delete;

	DescriptorPoolBuilder(Context& context)
		: m_Context{ context } {}

	~DescriptorPoolBuilder() = default;

	DescriptorPoolBuilder(DescriptorPoolBuilder&&)                 = delete;
	DescriptorPoolBuilder(DescriptorPoolBuilder const&)            = delete;
	DescriptorPoolBuilder& operator=(DescriptorPoolBuilder&&)      = delete;
	DescriptorPoolBuilder& operator=(DescriptorPoolBuilder const&) = delete;

	DescriptorPoolBuilder& AddPoolSize(VkDescriptorType type, uint32_t count);

	[[nodiscard]] DescriptorPool Build(uint32_t maxSets, bool addToQueue = true);

private:
	Context& m_Context;

	std::vector<VkDescriptorPoolSize> m_DescriptorPoolSizes{};
};

#endif //DESCRIPTOR_POOL_H
