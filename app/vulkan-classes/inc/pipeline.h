
#ifndef PIPELINE_H
#define PIPELINE_H
#include <span>

#include "pipeline_layout.h"
#include "shader_stage.h"

namespace vkc
{
	class Pipeline final
	{
	public:
		~Pipeline() = default;

		Pipeline(Pipeline&&)                 = default;
		Pipeline(Pipeline const&)            = delete;
		Pipeline& operator=(Pipeline&&)      = default;
		Pipeline& operator=(Pipeline const&) = delete;

		void Destroy(Context const& context) const;

		operator VkPipeline() const
		{
			return m_Pipeline;
		}

		operator VkPipeline*()
		{
			return &m_Pipeline;
		}

	private:
		friend class PipelineBuilder;
		Pipeline() = default;

		VkPipeline m_Pipeline{};
	};

	class PipelineBuilder final
	{
	public:
		PipelineBuilder() = delete;

		PipelineBuilder(Context& context);

		~PipelineBuilder() = default;

		PipelineBuilder(PipelineBuilder&&)                 = delete;
		PipelineBuilder(PipelineBuilder const&)            = delete;
		PipelineBuilder& operator=(PipelineBuilder&&)      = delete;
		PipelineBuilder& operator=(PipelineBuilder const&) = delete;

		PipelineBuilder& AddShaderStage(ShaderStage const& shaderStage);

		PipelineBuilder& SetRenderingAttachments(std::span<VkFormat> colorAttachmentFormats, VkFormat depthFormat, VkFormat stencilFormat);

		PipelineBuilder& SetVertexDescription
		(std::span<VkVertexInputBindingDescription> bindingDesc, std::span<VkVertexInputAttributeDescription> attributeDesc);

		PipelineBuilder& SetTopology(VkPrimitiveTopology topology);

		PipelineBuilder& AddViewport(VkExtent2D const& extent);

		PipelineBuilder& SetDepthBias(float constantFactor, float slopeFactor, float clamp = .0f, bool clampEnable = true);

		PipelineBuilder& SetCullMode(VkCullModeFlagBits cullMode);

		PipelineBuilder& SetFrontFace(VkFrontFace frontFace);

		PipelineBuilder& SetPolygonMode(VkPolygonMode polygonMode);

		PipelineBuilder& AddDynamicState(VkDynamicState dynamicState);

		PipelineBuilder& EnableDepthTest(VkCompareOp op, VkBool32 enable = VK_TRUE);

		PipelineBuilder& EnableDepthWrite(VkBool32 enable = VK_TRUE);

		[[nodiscard]] Pipeline Build(PipelineLayout const& layout, bool addToQueue = true);

	private:
		Context& m_Context;

		VkPipelineRenderingCreateInfo m_PipelineRendering{};

		std::vector<VkRect2D>                        m_Scissors{};
		std::vector<VkViewport>                      m_Viewports{};
		std::vector<VkPipelineShaderStageCreateInfo> m_ShaderStages{};
		std::vector<VkDynamicState>                  m_DynamicStates{};
		VkPipelineVertexInputStateCreateInfo         m_VertexInputState{};
		VkPipelineInputAssemblyStateCreateInfo       m_InputAssembly{};
		VkPipelineViewportStateCreateInfo            m_ViewportState{};
		VkPipelineRasterizationStateCreateInfo       m_Rasterizer{};
		VkPipelineMultisampleStateCreateInfo         m_MultisampleState{};
		VkPipelineColorBlendAttachmentState          m_ColorBlendAttachment{};
		VkPipelineColorBlendStateCreateInfo          m_ColorBlendState{};
		VkPipelineDepthStencilStateCreateInfo        m_DepthStencilState{};
	};
}

#endif //PIPELINE_H
