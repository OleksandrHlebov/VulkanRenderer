#ifndef DESCRIPTOR_SET_LAYOUT_H
#define DESCRIPTOR_SET_LAYOUT_H

#include "context.h"

class DescriptorSetLayout final
{
public:
	~DescriptorSetLayout() = default;

	DescriptorSetLayout(DescriptorSetLayout&&)                 = default;
	DescriptorSetLayout(DescriptorSetLayout const&)            = delete;
	DescriptorSetLayout& operator=(DescriptorSetLayout&&)      = default;
	DescriptorSetLayout& operator=(DescriptorSetLayout const&) = delete;

	void Destroy(Context const& context) const;

	operator VkDescriptorSetLayout() const
	{
		return m_Layout;
	}

	operator VkDescriptorSetLayout*()
	{
		return &m_Layout;
	}

private:
	friend class DescriptorSetLayoutBuilder;
	DescriptorSetLayout() = default;

	VkDescriptorSetLayout m_Layout{};
};

class DescriptorSetLayoutBuilder final
{
public:
	DescriptorSetLayoutBuilder() = delete;

	DescriptorSetLayoutBuilder(Context& context);

	~DescriptorSetLayoutBuilder() = default;

	DescriptorSetLayoutBuilder(DescriptorSetLayoutBuilder&&)                 = delete;
	DescriptorSetLayoutBuilder(DescriptorSetLayoutBuilder const&)            = delete;
	DescriptorSetLayoutBuilder& operator=(DescriptorSetLayoutBuilder&&)      = delete;
	DescriptorSetLayoutBuilder& operator=(DescriptorSetLayoutBuilder const&) = delete;

	DescriptorSetLayoutBuilder& AddBinding
	(uint32_t binding, VkDescriptorType descriptorType, VkShaderStageFlags stageFlags, uint32_t count = 1);

	[[nodiscard]] DescriptorSetLayout Build(bool addToQueue = true);

private:
	Context& m_Context;

	std::vector<VkDescriptorSetLayoutBinding> m_Bindings{};
};

#endif //DESCRIPTOR_SET_LAYOUT_H
