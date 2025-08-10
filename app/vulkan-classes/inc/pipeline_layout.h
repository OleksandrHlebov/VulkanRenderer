
#ifndef PIPELINE_LAYOUT_H
#define PIPELINE_LAYOUT_H

#include "context.h"

class PipelineLayout final
{
public:
	~PipelineLayout() = default;

	PipelineLayout(PipelineLayout&&)                 = default;
	PipelineLayout(PipelineLayout const&)            = delete;
	PipelineLayout& operator=(PipelineLayout&&)      = default;
	PipelineLayout& operator=(PipelineLayout const&) = delete;

	void Destroy(Context const& context) const;

	operator VkPipelineLayout() const
	{
		return m_Layout;
	}

	operator VkPipelineLayout*()
	{
		return &m_Layout;
	}

private:
	friend class PipelineLayoutBuilder;
	PipelineLayout() = default;

	VkPipelineLayout m_Layout{};
};

class PipelineLayoutBuilder final
{
public:
	PipelineLayoutBuilder() = delete;

	PipelineLayoutBuilder(Context& context)
		: m_Context{ context } {}

	~PipelineLayoutBuilder() = default;

	PipelineLayoutBuilder(PipelineLayoutBuilder&&)                 = delete;
	PipelineLayoutBuilder(PipelineLayoutBuilder const&)            = delete;
	PipelineLayoutBuilder& operator=(PipelineLayoutBuilder&&)      = delete;
	PipelineLayoutBuilder& operator=(PipelineLayoutBuilder const&) = delete;

	PipelineLayoutBuilder& AddDescriptorSetLayout(VkDescriptorSetLayout layout);

	PipelineLayoutBuilder& AddPushConstant(VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size);

	[[nodiscard]] PipelineLayout Build(bool addToQueue = true) const;

private:
	Context& m_Context;

	std::vector<VkDescriptorSetLayout> m_DescSetLayouts{};
	std::vector<VkPushConstantRange>   m_PushConstantRanges{};
};

#endif //PIPELINE_LAYOUT_H
