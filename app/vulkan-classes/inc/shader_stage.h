#ifndef SHADER_STAGE_H
#define SHADER_STAGE_H
#include <span>

#include "context.h"

class ShaderStage final
{
public:
	ShaderStage() = delete;

	ShaderStage(Context const& context, std::vector<char>&& code, VkShaderStageFlagBits stage)
		: ShaderStage(context, code, stage) {}

	ShaderStage(Context const& context, std::span<char> code, VkShaderStageFlagBits stage)
		: m_Context{ context }
	{
		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = code.size();
		createInfo.pCode    = reinterpret_cast<uint32_t const*>(code.data());

		if (auto const result = context.DispatchTable.createShaderModule(&createInfo, nullptr, &m_Module);
			result != VK_SUCCESS)
			throw std::runtime_error("failed to create shader module");

		m_Info.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		m_Info.stage  = stage;
		m_Info.module = m_Module;
		m_Info.pName  = "main";
	}

	~ShaderStage()
	{
		m_Context.DispatchTable.destroyShaderModule(m_Module, nullptr);
	}

	ShaderStage(ShaderStage&&)                 = delete;
	ShaderStage(ShaderStage const&)            = delete;
	ShaderStage& operator=(ShaderStage&&)      = delete;
	ShaderStage& operator=(ShaderStage const&) = delete;

	template<typename DataType>
	void AddSpecializationConstant(DataType data)
	{
		auto const offset = static_cast<uint32_t>(m_SpecializationData.size());
		m_MapEntries.emplace_back(static_cast<uint32_t>(m_MapEntries.size())
								  , offset
								  , sizeof(data));

		// char size is 1 byte, hence we allocate sizeof(data) amount of chars
		m_SpecializationData.insert(m_SpecializationData.end(), sizeof(data), 0);
		std::memcpy(&m_SpecializationData[offset], &data, sizeof(data)); // copy data into the allocated memory

		m_SpecializationInfo.dataSize      = static_cast<uint32_t>(m_SpecializationData.size());
		m_SpecializationInfo.pData         = m_SpecializationData.data();
		m_SpecializationInfo.mapEntryCount = static_cast<uint32_t>(m_MapEntries.size());
		m_SpecializationInfo.pMapEntries   = m_MapEntries.data();

		m_Info.pSpecializationInfo = &m_SpecializationInfo;
	}

	operator VkPipelineShaderStageCreateInfo() const
	{
		return m_Info;
	}

private:
	VkShaderModule                  m_Module{};
	VkPipelineShaderStageCreateInfo m_Info{};
	VkSpecializationInfo            m_SpecializationInfo{};

	std::vector<char>                     m_SpecializationData{};
	std::vector<VkSpecializationMapEntry> m_MapEntries{};

	Context const& m_Context;
};

#endif //SHADER_STAGE_H
