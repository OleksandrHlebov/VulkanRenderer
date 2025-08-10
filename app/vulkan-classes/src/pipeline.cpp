#include "pipeline.h"

#include "shader_stage.h"

void vkc::Pipeline::Destroy(Context const& context) const
{
	context.DispatchTable.destroyPipeline(m_Pipeline, nullptr);
}

vkc::PipelineBuilder::PipelineBuilder(Context& context)
	: m_Context{ context }
{
	m_VertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	m_PipelineRendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;

	m_InputAssembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	m_InputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	m_ViewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;

	m_Rasterizer.sType     = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	m_Rasterizer.lineWidth = 1.0f;

	m_MultisampleState.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	m_MultisampleState.sampleShadingEnable  = VK_FALSE;
	m_MultisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	m_ColorBlendAttachment.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	m_ColorBlendAttachment.blendEnable = VK_FALSE;

	m_ColorBlendState.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	m_ColorBlendState.logicOpEnable   = VK_FALSE;
	m_ColorBlendState.attachmentCount = 1;
	m_ColorBlendState.pAttachments    = &m_ColorBlendAttachment;

	m_DepthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
}

vkc::PipelineBuilder& vkc::PipelineBuilder::AddShaderStage(ShaderStage const& shaderStage)
{
	m_ShaderStages.emplace_back(shaderStage);
	return *this;
}

vkc::PipelineBuilder& vkc::PipelineBuilder::SetRenderingAttachments
(std::span<VkFormat> colorAttachmentFormats, VkFormat depthFormat, VkFormat stencilFormat)
{
	m_PipelineRendering.colorAttachmentCount = static_cast<uint32_t>(colorAttachmentFormats.size());
	// ReSharper disable once CppDFALocalValueEscapesFunction
	// span is a view to a container, it's user's responsibility to manage is lifetime
	m_PipelineRendering.pColorAttachmentFormats = colorAttachmentFormats.data();
	m_PipelineRendering.depthAttachmentFormat   = depthFormat;
	m_PipelineRendering.stencilAttachmentFormat = stencilFormat;
	return *this;
}

vkc::PipelineBuilder& vkc::PipelineBuilder::SetVertexDescription
(std::span<VkVertexInputBindingDescription> bindingDesc, std::span<VkVertexInputAttributeDescription> attributeDesc)
{
	m_VertexInputState.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	m_VertexInputState.pNext                           = nullptr;
	m_VertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDesc.size());
	m_VertexInputState.pVertexAttributeDescriptions    = attributeDesc.data();
	m_VertexInputState.vertexBindingDescriptionCount   = static_cast<uint32_t>(bindingDesc.size());
	m_VertexInputState.pVertexBindingDescriptions      = bindingDesc.data();

	return *this;
}

vkc::PipelineBuilder& vkc::PipelineBuilder::SetTopology(VkPrimitiveTopology topology)
{
	m_InputAssembly.topology = topology;
	return *this;
}

vkc::PipelineBuilder& vkc::PipelineBuilder::AddViewport(VkExtent2D const& extent)
{
	VkViewport viewport{};
	viewport.width    = static_cast<float>(extent.width);
	viewport.height   = static_cast<float>(extent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	m_Viewports.emplace_back(viewport);

	VkRect2D scissor{};
	scissor.extent = extent;
	scissor.offset = { 0, 0 };
	m_Scissors.emplace_back(scissor);
	return *this;
}

vkc::PipelineBuilder& vkc::PipelineBuilder::SetDepthBias(float constantFactor, float slopeFactor, float clamp, bool clampEnable)
{
	m_Rasterizer.depthClampEnable        = clampEnable;
	m_Rasterizer.depthBiasConstantFactor = constantFactor;
	m_Rasterizer.depthBiasSlopeFactor    = slopeFactor;
	m_Rasterizer.depthBiasClamp          = clamp;
	return *this;
}

vkc::PipelineBuilder& vkc::PipelineBuilder::SetCullMode(VkCullModeFlagBits cullMode)
{
	m_Rasterizer.cullMode = cullMode;
	return *this;
}

vkc::PipelineBuilder& vkc::PipelineBuilder::SetFrontFace(VkFrontFace frontFace)
{
	m_Rasterizer.frontFace = frontFace;
	return *this;
}

vkc::PipelineBuilder& vkc::PipelineBuilder::SetPolygonMode(VkPolygonMode polygonMode)
{
	m_Rasterizer.polygonMode = polygonMode;
	return *this;
}

vkc::PipelineBuilder& vkc::PipelineBuilder::AddDynamicState(VkDynamicState dynamicState)
{
	m_DynamicStates.emplace_back(dynamicState);
	return *this;
}

vkc::PipelineBuilder& vkc::PipelineBuilder::EnableDepthTest(VkCompareOp op, VkBool32 enable)
{
	m_DepthStencilState.depthTestEnable = enable;
	m_DepthStencilState.depthCompareOp  = op;
	return *this;
}

vkc::PipelineBuilder& vkc::PipelineBuilder::EnableDepthWrite(VkBool32 enable)
{
	m_DepthStencilState.depthWriteEnable = enable;
	return *this;
}

vkc::Pipeline vkc::PipelineBuilder::Build(PipelineLayout const& layout, bool addToQueue)
{
	Pipeline pipeline{};

	m_ViewportState.viewportCount = static_cast<uint32_t>(m_Viewports.size());
	m_ViewportState.pViewports    = m_Viewports.data();
	m_ViewportState.scissorCount  = static_cast<uint32_t>(m_Scissors.size());
	m_ViewportState.pScissors     = m_Scissors.data();

	VkPipelineDynamicStateCreateInfo pipelineDynamicState{};
	pipelineDynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	pipelineDynamicState.dynamicStateCount = static_cast<uint32_t>(m_DynamicStates.size());
	pipelineDynamicState.pDynamicStates    = m_DynamicStates.data();

	VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
	pipelineCreateInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.pNext               = &m_PipelineRendering;
	pipelineCreateInfo.stageCount          = static_cast<uint32_t>(m_ShaderStages.size());
	pipelineCreateInfo.pStages             = m_ShaderStages.data();
	pipelineCreateInfo.pVertexInputState   = &m_VertexInputState;
	pipelineCreateInfo.pInputAssemblyState = &m_InputAssembly;
	pipelineCreateInfo.pViewportState      = &m_ViewportState;
	pipelineCreateInfo.pRasterizationState = &m_Rasterizer;
	pipelineCreateInfo.pMultisampleState   = &m_MultisampleState;
	pipelineCreateInfo.pColorBlendState    = &m_ColorBlendState;
	pipelineCreateInfo.pDynamicState       = &pipelineDynamicState;
	pipelineCreateInfo.pDepthStencilState  = &m_DepthStencilState;
	pipelineCreateInfo.renderPass          = VK_NULL_HANDLE;
	pipelineCreateInfo.layout              = layout;

	if (m_Context.DispatchTable.createGraphicsPipelines(VK_NULL_HANDLE
														, 1
														, &pipelineCreateInfo
														, nullptr
														, pipeline) != VK_SUCCESS)
		throw std::runtime_error("failed to create graphics pipeline");

	if (addToQueue)
		m_Context.DeletionQueue.
				  Push([context = &m_Context, pipeline = pipeline.m_Pipeline]
				  {
					  context->DispatchTable.destroyPipeline(pipeline, nullptr);
				  });

	return pipeline;
}
