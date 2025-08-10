#include "pipeline_layout.h"

void vkc::PipelineLayout::Destroy(Context const& context) const
{
	context.DispatchTable.destroyPipelineLayout(m_Layout, nullptr);
}

vkc::PipelineLayoutBuilder& vkc::PipelineLayoutBuilder::AddDescriptorSetLayout(VkDescriptorSetLayout layout)
{
	m_DescSetLayouts.emplace_back(layout);
	return *this;
}

vkc::PipelineLayoutBuilder& vkc::PipelineLayoutBuilder::AddPushConstant(VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size)
{
	m_PushConstantRanges.emplace_back(stageFlags, offset, size);
	return *this;
}

vkc::PipelineLayout vkc::PipelineLayoutBuilder::Build(bool addToQueue) const
{
	PipelineLayout layout{};

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount         = static_cast<uint32_t>(m_DescSetLayouts.size());
	pipelineLayoutInfo.pSetLayouts            = m_DescSetLayouts.data();
	pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(m_PushConstantRanges.size());
	pipelineLayoutInfo.pPushConstantRanges    = m_PushConstantRanges.data();

	m_Context.DispatchTable.createPipelineLayout(&pipelineLayoutInfo, nullptr, layout);
	if (addToQueue)
		m_Context.DeletionQueue.Push([context = &m_Context, layout = layout.m_Layout]
		{
			context->DispatchTable.destroyPipelineLayout(layout, nullptr);
		});

	return layout;
}
