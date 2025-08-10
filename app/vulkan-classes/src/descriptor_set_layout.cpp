#include "descriptor_set_layout.h"

void DescriptorSetLayout::Destroy(Context const& context) const
{
	context.DispatchTable.destroyDescriptorSetLayout(m_Layout, nullptr);
}

DescriptorSetLayoutBuilder::DescriptorSetLayoutBuilder(Context& context)
	: m_Context{ context } {}

DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::AddBinding
(uint32_t binding, VkDescriptorType descriptorType, VkShaderStageFlags stageFlags, uint32_t count)
{
	VkDescriptorSetLayoutBinding bindingInfo{};
	bindingInfo.binding         = binding;
	bindingInfo.descriptorType  = descriptorType;
	bindingInfo.stageFlags      = stageFlags;
	bindingInfo.descriptorCount = count;
	m_Bindings.emplace_back(bindingInfo);
	return *this;
}

DescriptorSetLayout DescriptorSetLayoutBuilder::Build(bool addToQueue)
{
	DescriptorSetLayout             layout{};
	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(m_Bindings.size());
	layoutInfo.pBindings    = m_Bindings.data();
	if (m_Context.DispatchTable.createDescriptorSetLayout(&layoutInfo, nullptr, layout) != VK_SUCCESS)
		throw std::runtime_error("Failed to create frame descriptor set layout");

	if (addToQueue)
		m_Context.DeletionQueue.Push([context = &m_Context, layout = layout.m_Layout]
		{
			context->DispatchTable.destroyDescriptorSetLayout(layout, nullptr);
		});
	m_Bindings.clear();
	return layout;
}
