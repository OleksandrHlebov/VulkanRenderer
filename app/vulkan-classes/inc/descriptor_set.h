#ifndef DESCRIPTOR_SET_H
#define DESCRIPTOR_SET_H

#include <span>

#include "context.h"

class DescriptorSet final
{
public:
	~DescriptorSet() = default;

	DescriptorSet(DescriptorSet&&)                 = default;
	DescriptorSet(DescriptorSet const&)            = delete;
	DescriptorSet& operator=(DescriptorSet&&)      = default;
	DescriptorSet& operator=(DescriptorSet const&) = delete;

	DescriptorSet& AddWriteDescriptor
	(std::span<VkDescriptorImageInfo> imageInfos, VkDescriptorType type, uint32_t binding, uint32_t arrayElement);

	DescriptorSet& AddWriteDescriptor
	(std::span<VkDescriptorBufferInfo> bufferInfos, VkDescriptorType type, uint32_t binding, uint32_t arrayElement);

	void Update(Context const& context);

	operator VkDescriptorSet() const
	{
		return m_Set;
	}

	operator VkDescriptorSet*()
	{
		return &m_Set;
	}

private:
	friend class DescriptorSetBuilder;

	DescriptorSet(VkDescriptorSet set)
		: m_Set{ set } {}

	VkDescriptorSet m_Set;

	std::vector<VkWriteDescriptorSet> m_WriteDescriptorSets{};
};

class DescriptorSetBuilder final
{
public:
	DescriptorSetBuilder() = delete;

	DescriptorSetBuilder(Context& context);

	~DescriptorSetBuilder() = default;

	DescriptorSetBuilder(DescriptorSetBuilder&&)                 = delete;
	DescriptorSetBuilder(DescriptorSetBuilder const&)            = delete;
	DescriptorSetBuilder& operator=(DescriptorSetBuilder&&)      = delete;
	DescriptorSetBuilder& operator=(DescriptorSetBuilder const&) = delete;

	[[nodiscard]] std::vector<DescriptorSet> Build(VkDescriptorPool pool, std::span<VkDescriptorSetLayout> layouts) const;

private:
	Context& m_Context;
};

#endif //DESCRIPTOR_SET_H
