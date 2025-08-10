#include "descriptor_set.h"

DescriptorSet& DescriptorSet::AddWriteDescriptor
(std::span<VkDescriptorImageInfo> imageInfos, VkDescriptorType type, uint32_t binding, uint32_t arrayElement)
{
	VkWriteDescriptorSet writeDescriptorSet{};
	writeDescriptorSet.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDescriptorSet.dstSet          = *this;
	writeDescriptorSet.dstBinding      = binding;
	writeDescriptorSet.dstArrayElement = arrayElement;
	writeDescriptorSet.descriptorType  = type;
	writeDescriptorSet.descriptorCount = static_cast<uint32_t>(imageInfos.size());
	writeDescriptorSet.pImageInfo      = imageInfos.data();
	writeDescriptorSet.pBufferInfo     = nullptr;

	m_WriteDescriptorSets.emplace_back(writeDescriptorSet);
	return *this;
}

DescriptorSet& DescriptorSet::AddWriteDescriptor
(std::span<VkDescriptorBufferInfo> bufferInfos, VkDescriptorType type, uint32_t binding, uint32_t arrayElement)
{
	VkWriteDescriptorSet writeDescriptorSet{};
	writeDescriptorSet.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDescriptorSet.dstSet          = *this;
	writeDescriptorSet.dstBinding      = binding;
	writeDescriptorSet.dstArrayElement = arrayElement;
	writeDescriptorSet.descriptorType  = type;
	writeDescriptorSet.descriptorCount = static_cast<uint32_t>(bufferInfos.size());
	writeDescriptorSet.pImageInfo      = nullptr;
	writeDescriptorSet.pBufferInfo     = bufferInfos.data();

	m_WriteDescriptorSets.emplace_back(writeDescriptorSet);
	return *this;
}

void DescriptorSet::Update(Context const& context)
{
	context.DispatchTable.updateDescriptorSets(static_cast<uint32_t>(m_WriteDescriptorSets.size())
											   , m_WriteDescriptorSets.data()
											   , 0
											   , nullptr);
	m_WriteDescriptorSets.clear();
}

DescriptorSetBuilder::DescriptorSetBuilder(Context& context)
	: m_Context{ context } {}

std::vector<DescriptorSet> DescriptorSetBuilder::Build
(VkDescriptorPool pool, std::span<VkDescriptorSetLayout> layouts) const
{
	std::vector<VkDescriptorSet> sets(layouts.size());

	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool     = pool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
	allocInfo.pSetLayouts        = layouts.data();

	if (auto const result = m_Context.DispatchTable.allocateDescriptorSets(&allocInfo, sets.data());
		result != VK_SUCCESS)
		throw std::runtime_error("Failed to allocate descriptor sets");

	std::vector<DescriptorSet> output;
	for (auto const& set: sets)
		output.emplace_back(DescriptorSet{ set });
	return output;
}
