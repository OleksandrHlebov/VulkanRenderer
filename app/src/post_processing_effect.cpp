#include "post_processing_effect.h"
#include "pipeline.h"
#include "pipeline_layout.h"
#include "descriptor_set.h"
#include "descriptor_set_layout.h"
#include "image.h"
#include "helper.h"
#include "pingpong_render_target.h"

PostProcessingEffect::PostProcessingEffect
(
	vkc::Context&                  context, DescriptorData const& descriptorData, vkc::PipelineCache& pipelineCache, Type type
	, std::filesystem::path const& shaderPath
)
	: m_Name{ shaderPath.stem().string() }
	, m_DescriptorData{ descriptorData }
{
	//
	{
		vkc::PipelineLayoutBuilder builder{ context };

		m_PipelineLayout = std::make_unique<vkc::PipelineLayout>(builder
																 .AddDescriptorSetLayout(descriptorData.GlobalSetLayout)
																 .AddDescriptorSetLayout(descriptorData.GBufferSetLayout)
																 .AddPushConstant(VK_SHADER_STAGE_FRAGMENT_BIT
																				  , 0
																				  , sizeof(uint64_t) + sizeof(uint32_t))
																 .Build());
	}

	//
	{
		vkc::ShaderStage quad{ context, help::ReadFile("shaders/quad.spv"), VK_SHADER_STAGE_VERTEX_BIT };
		vkc::ShaderStage effect{ context, help::ReadFile(shaderPath.string()), VK_SHADER_STAGE_FRAGMENT_BIT };

		VkFormat colorAttachmentFormats[1]{};
		switch (type)
		{
		case Type::SDR:
			colorAttachmentFormats[0] = context.Swapchain.image_format;
			break;
		case Type::HDR:
			colorAttachmentFormats[0] = VK_FORMAT_R32G32B32A32_SFLOAT;
			break;
		default: ;
		}

		VkPipelineColorBlendAttachmentState blendAttachment{};
		blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT
										 | VK_COLOR_COMPONENT_G_BIT
										 | VK_COLOR_COMPONENT_B_BIT
										 | VK_COLOR_COMPONENT_A_BIT;

		vkc::PipelineBuilder builder{ context };
		vkc::Pipeline        pipeline = builder
								 .AddViewport(context.Swapchain.extent)
								 .SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
								 .SetPolygonMode(VK_POLYGON_MODE_FILL)
								 .SetCullMode(VK_CULL_MODE_NONE)
								 .SetFrontFace(VK_FRONT_FACE_COUNTER_CLOCKWISE)
								 .AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
								 .AddDynamicState(VK_DYNAMIC_STATE_SCISSOR)
								 .AddColorBlendAttachment(blendAttachment)
								 .SetRenderingAttachments(colorAttachmentFormats, VK_FORMAT_UNDEFINED, VK_FORMAT_UNDEFINED)
								 .UseCache(pipelineCache)
								 .AddShaderStage(quad)
								 .AddShaderStage(effect)
								 .Build(*m_PipelineLayout, true);
		m_Pipeline = std::make_unique<vkc::Pipeline>(std::move(pipeline));
	}

	m_PushConstants.resize(sizeof(uint64_t) + sizeof(uint32_t));
}

void PostProcessingEffect::Render
(
	vkc::Context& context, vkc::CommandBuffer& commandBuffer, RenderData& renderData, uint32_t frameIndex, bool renderToSwapchain
)
{
	// VkDebugUtilsLabelEXT debugLabel{};
	// debugLabel.sType      = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
	// debugLabel.pLabelName = "blit pass";
	// static float constexpr color[4]{ .23f, 1.f, .65f, 1.f };
	// for (size_t index{}; index < std::size(debugLabel.color); ++index)
	// 	debugLabel.color[index] = color[index];
	// context.DispatchTable.cmdBeginDebugUtilsLabelEXT(commandBuffer, &debugLabel);

	auto [lastImage, lastView] = renderData.PingPongTarget.AcquireLastRenderedToTarget();

	vkc::Image& renderTarget = renderToSwapchain
							   ? renderData.SwapchainImage
							   : *renderData.PingPongTarget.AcquireNextTarget().first;
	vkc::ImageView& renderView = renderToSwapchain
								 ? renderData.SwapchainImageView
								 : *renderData.PingPongTarget.AcquireCurrentTarget().second;

	// render target image to attachment optimal
	{
		vkc::Image::Transition transition{};
		//
		{
			transition.SrcAccessMask = VK_ACCESS_2_NONE;
			transition.DstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			transition.SrcStageMask  = VK_PIPELINE_STAGE_2_NONE;
			transition.DstStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
			transition.NewLayout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}
		renderTarget.MakeTransition(context, commandBuffer, transition);
	}
	//
	{
		vkc::Image::Transition transition{};
		//
		{
			transition.SrcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
			transition.DstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;
			transition.SrcStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
			transition.DstStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
			transition.NewLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}
		lastImage->MakeTransition(context, commandBuffer, transition);
	}

	VkRenderingAttachmentInfo renderingAttachmentInfo{};
	renderingAttachmentInfo.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	renderingAttachmentInfo.clearValue  = { { .03f, .03f, .03f, 1.f } };
	renderingAttachmentInfo.imageLayout = renderTarget.GetLayout();
	renderingAttachmentInfo.imageView   = renderView;
	renderingAttachmentInfo.loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR;
	renderingAttachmentInfo.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;

	VkRenderingInfo renderingInfo{};
	renderingInfo.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO;
	renderingInfo.colorAttachmentCount = 1;
	renderingInfo.pColorAttachments    = &renderingAttachmentInfo;
	renderingInfo.layerCount           = 1;
	renderingInfo.renderArea           = VkRect2D{ {}, context.Swapchain.extent };

	context.DispatchTable.cmdBeginRendering(commandBuffer, &renderingInfo);
	// render
	{
		VkViewport viewport{};
		viewport.width    = static_cast<float>(context.Swapchain.extent.width);
		viewport.height   = static_cast<float>(context.Swapchain.extent.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		context.DispatchTable.cmdSetViewport(commandBuffer, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = context.Swapchain.extent;

		context.DispatchTable.cmdSetScissor(commandBuffer, 0, 1, &scissor);

		VkDescriptorSet const sets[]{
			m_DescriptorData.GlobalSet[frameIndex]
			, m_DescriptorData.GBufferSet[frameIndex]
		};

		context.DispatchTable.cmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_Pipeline);
		context.DispatchTable.cmdBindDescriptorSets(commandBuffer
													, VK_PIPELINE_BIND_POINT_GRAPHICS
													, *m_PipelineLayout
													, 0
													, static_cast<uint32_t>(std::size(sets))
													, sets
													, 0
													, nullptr);
		uint64_t time  = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		uint32_t index = renderData.PingPongTarget.GetLastImageIndex();
		std::memcpy(m_PushConstants.data(), &time, sizeof(time));
		std::memcpy(std::next(m_PushConstants.data(), sizeof(time)), &index, sizeof(index));
		context.DispatchTable.cmdPushConstants(commandBuffer
											   , *m_PipelineLayout
											   , VK_SHADER_STAGE_FRAGMENT_BIT
											   , 0
											   , static_cast<uint32_t>(m_PushConstants.size() * sizeof(m_PushConstants[0]))
											   , m_PushConstants.data());

		context.DispatchTable.cmdDraw(commandBuffer, 3, 1, 0, 0);
	}
	context.DispatchTable.cmdEndRendering(commandBuffer);
	// context.DispatchTable.cmdEndDebugUtilsLabelEXT(commandBuffer);
}
