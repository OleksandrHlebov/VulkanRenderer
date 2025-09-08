#ifndef VULKANRESEARCH_SHADOW_GENERATION_H
#define VULKANRESEARCH_SHADOW_GENERATION_H

#include "datatypes.h"
#include "scene.h"

namespace shadow
{
	inline std::pair<vkc::PipelineLayout, vkc::Pipeline> CreatePipelineForDirectionalShadows
	(vkc::Context& context, vkc::DescriptorSetLayout const& descSetLayout, VkFormat depthFormat, VkExtent2D shadowRes)
	{
		vkc::PipelineLayoutBuilder layoutBuilder{ context };
		vkc::PipelineLayout        directionalPipelineLayout = layoutBuilder
														.AddPushConstant(VK_SHADER_STAGE_FRAGMENT_BIT
																		 , 0
																		 , sizeof(uint32_t))
														.AddPushConstant(VK_SHADER_STAGE_VERTEX_BIT, 16, sizeof(glm::mat4))
														.AddDescriptorSetLayout(descSetLayout)
														.Build(false);

		vkc::ShaderStage const vert{ context, help::ReadFile("shaders/transform_to_lightspace.spv"), VK_SHADER_STAGE_VERTEX_BIT };
		vkc::ShaderStage const frag{ context, help::ReadFile("shaders/alpha_discard.spv"), VK_SHADER_STAGE_FRAGMENT_BIT };

		vkc::PipelineBuilder pipelineBuilder{ context };
		vkc::Pipeline        directionalPipeline = pipelineBuilder
											.AddShaderStage(vert)
											.AddShaderStage(frag)
											.AddViewport(shadowRes)
											.EnableDepthTest(VK_COMPARE_OP_LESS)
											.EnableDepthWrite()
											.SetDepthBias(1.25f, 1.f)
											.SetFrontFace(VK_FRONT_FACE_COUNTER_CLOCKWISE)
											.SetCullMode(VK_CULL_MODE_BACK_BIT)
											.SetPolygonMode(VK_POLYGON_MODE_FILL)
											.SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
											.SetRenderingAttachments({}
																	 , depthFormat
																	 , VK_FORMAT_UNDEFINED)
											.SetVertexDescription(Vertex::GetBindingDescription(), Vertex::GetAttributeDescription())
											.Build(directionalPipelineLayout, false);
		return { std::move(directionalPipelineLayout), std::move(directionalPipeline) };
	}

	inline std::pair<vkc::PipelineLayout, vkc::Pipeline> CreatePipelineForPointShadows
	(vkc::Context& context, vkc::DescriptorSetLayout const& descSetLayout, VkFormat depthFormat, VkExtent2D shadowRes)
	{
		vkc::ShaderStage vert{ context, help::ReadFile("shaders/transform_to_lightspace.spv"), VK_SHADER_STAGE_VERTEX_BIT };
		vkc::ShaderStage depthOverride{ context, help::ReadFile("shaders/frag_depth_override.spv"), VK_SHADER_STAGE_FRAGMENT_BIT };

		vkc::PipelineLayoutBuilder layoutBuilder{ context };
		vkc::PipelineLayout        pointPipelineLayout = layoutBuilder
												  .AddPushConstant(VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT
																   , 0
																   , sizeof(glm::vec4) + sizeof(glm::mat4) + sizeof(uint32_t))
												  .AddDescriptorSetLayout(descSetLayout)
												  .Build(false);
		vkc::PipelineBuilder pipelineBuilder{ context };
		vkc::Pipeline        pointPipeline = pipelineBuilder
									  .AddShaderStage(vert)
									  .AddShaderStage(depthOverride)
									  .AddViewport(shadowRes)
									  .EnableDepthTest(VK_COMPARE_OP_LESS)
									  .EnableDepthWrite()
									  .SetDepthBias(1.25f, 1.f)
									  .SetFrontFace(VK_FRONT_FACE_COUNTER_CLOCKWISE)
									  .SetCullMode(VK_CULL_MODE_BACK_BIT)
									  .SetPolygonMode(VK_POLYGON_MODE_FILL)
									  .SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
									  .SetRenderingAttachments({}
															   , depthFormat
															   , VK_FORMAT_UNDEFINED)
									  .SetVertexDescription(Vertex::GetBindingDescription(), Vertex::GetAttributeDescription())
									  .Build(pointPipelineLayout, false);

		return { std::move(pointPipelineLayout), std::move(pointPipeline) };
	}

	inline void RecordPointShadowsGeneration
	(
		vkc::Context const&                      context
		, vkc::CommandBuffer const&              commandBuffer
		, Scene&                                 scene
		, std::span<vkc::Image>                  shadowMaps
		, std::span<std::vector<vkc::ImageView>> shadowMapViews
		, FrameData const&                       frameData
	)
	{
		auto pointLights = scene.GetPointLights();
		for (uint32_t lightIndex{}; lightIndex < pointLights.size(); ++lightIndex)
		{
			auto& light = pointLights[lightIndex];
			assert(light.IsPoint());

			glm::vec3 eye             = light.GetPosition();
			glm::mat4 captureViews[6] = {
				glm::lookAt(eye, eye + glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f))    // +X
				, glm::lookAt(eye, eye + glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)) // -X
				, glm::lookAt(eye, eye + glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f))   // +Y
				, glm::lookAt(eye, eye + glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)) // -Y
				, glm::lookAt(eye, eye + glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f))  // +Z
				, glm::lookAt(eye, eye + glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f)) // -Z
			};
			glm::mat4 captureProj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, App::SHADOW_FAR_PLANE);

			auto& shadowMap   = shadowMaps[lightIndex];
			auto& shadowViews = shadowMapViews[lightIndex];
			// transition to depth attachment optimal
			{
				vkc::Image::Transition transition{};
				transition.NewLayout     = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
				transition.SrcAccessMask = VK_ACCESS_NONE;
				transition.DstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
				transition.SrcStageMask  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
				transition.DstStageMask  = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
				transition.LayerCount    = 6;
				shadowMap.MakeTransition(context, commandBuffer, transition);
			}
			for (uint32_t faceIndex{}; faceIndex < 6; ++faceIndex)
			{
				auto&                     shadowView = shadowViews[faceIndex];
				VkRenderingAttachmentInfo depthAttachment{};
				depthAttachment.sType                   = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
				depthAttachment.clearValue.depthStencil = { 1.f, 0 };
				depthAttachment.imageLayout             = shadowMap.GetLayout(shadowView.GetBaseLayer(), shadowView.GetBaseMipLevel());
				depthAttachment.imageView               = shadowView;
				depthAttachment.loadOp                  = VK_ATTACHMENT_LOAD_OP_CLEAR;
				depthAttachment.storeOp                 = VK_ATTACHMENT_STORE_OP_STORE;

				VkRenderingInfo renderingInfo{};
				renderingInfo.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO;
				renderingInfo.colorAttachmentCount = 0;
				renderingInfo.pDepthAttachment     = &depthAttachment;
				renderingInfo.renderArea           = { {}, shadowMap.GetExtent() };
				renderingInfo.layerCount           = 1;

				context.DispatchTable.cmdBeginRendering(commandBuffer, &renderingInfo);

				context.DispatchTable.cmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, frameData.Pipeline);
				context.DispatchTable.cmdBindDescriptorSets(commandBuffer
															, VK_PIPELINE_BIND_POINT_GRAPHICS
															, frameData.PipelineLayout
															, 0
															, static_cast<uint32_t>(frameData.DescriptorSets.size())
															, frameData.DescriptorSets.data()
															, 0
															, nullptr);

				glm::mat4 const lightSpace = captureProj * captureViews[faceIndex];
				context.DispatchTable.cmdPushConstants(commandBuffer
													   , frameData.PipelineLayout
													   , VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT
													   , 16
													   , sizeof(glm::mat4)
													   , &lightSpace);
				auto const positionFar = glm::vec4{ glm::vec3{ light.GetPosition() }, App::SHADOW_FAR_PLANE };
				context.DispatchTable.cmdPushConstants(commandBuffer
													   , frameData.PipelineLayout
													   , VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT
													   , 0
													   , sizeof(glm::vec4)
													   , &positionFar);
				for (auto& meshes = scene.GetMeshes();
					 auto& mesh: meshes)
				{
					VkDeviceSize offsets[] = { 0 };
					context.DispatchTable.cmdPushConstants(commandBuffer
														   , frameData.PipelineLayout
														   , VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT
														   , sizeof(glm::mat4) + sizeof(glm::vec4)
														   , sizeof(uint32_t)
														   , &mesh.GetTextureIndices().Diffuse);

					context.DispatchTable.cmdBindVertexBuffers(commandBuffer
															   , 0
															   , 1
															   , mesh.GetVertexBuffer()
															   , offsets);

					context.DispatchTable.cmdBindIndexBuffer(commandBuffer, mesh.GetIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

					context.DispatchTable.cmdDrawIndexed(commandBuffer
														 , static_cast<uint32_t>(mesh.GetIndexBuffer().GetSize() / sizeof(uint32_t))
														 , 1
														 , 0
														 , 0
														 , 0);
				}
				context.DispatchTable.cmdEndRendering(commandBuffer);
				//
				{
					vkc::Image::Transition transition{ shadowView };
					transition.NewLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
					transition.SrcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
					transition.DstAccessMask = VK_ACCESS_NONE;
					transition.SrcStageMask  = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
					transition.DstStageMask  = VK_PIPELINE_STAGE_NONE;
					shadowMap.MakeTransition(context, commandBuffer, transition);
				}
			}
		}
	}

	inline void RecordDirectionalShadowsGeneration
	(
		vkc::Context const&         context
		, vkc::CommandBuffer const& commandBuffer
		, Scene&                    scene
		, std::span<vkc::Image>     shadowMaps
		, std::span<vkc::ImageView> shadowMapViews
		, FrameData const&          frameData
	)
	{
		for (uint32_t index{}; index < shadowMaps.size(); ++index)
		{
			auto& shadowMap  = shadowMaps[index];
			auto& shadowView = shadowMapViews[index];
			// transition to depth attachment optimal
			{
				vkc::Image::Transition transition{ shadowMapViews[index] };
				transition.NewLayout     = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
				transition.SrcAccessMask = VK_ACCESS_NONE;
				transition.DstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
				transition.SrcStageMask  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
				transition.DstStageMask  = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
				shadowMap.MakeTransition(context, commandBuffer, transition);
			}
			VkRenderingAttachmentInfo depthAttachment{};
			depthAttachment.sType                   = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
			depthAttachment.clearValue.depthStencil = { 1.f, 0 };
			depthAttachment.imageLayout             = shadowMap.GetLayout();
			depthAttachment.imageView               = shadowView;
			depthAttachment.loadOp                  = VK_ATTACHMENT_LOAD_OP_CLEAR;
			depthAttachment.storeOp                 = VK_ATTACHMENT_STORE_OP_STORE;

			VkRenderingInfo renderingInfo{};
			renderingInfo.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO;
			renderingInfo.colorAttachmentCount = 0;
			renderingInfo.pDepthAttachment     = &depthAttachment;
			renderingInfo.renderArea           = { {}, shadowMap.GetExtent() };
			renderingInfo.layerCount           = 1;

			context.DispatchTable.cmdBeginRendering(commandBuffer, &renderingInfo);

			context.DispatchTable.cmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, frameData.Pipeline);
			context.DispatchTable.cmdBindDescriptorSets(commandBuffer
														, VK_PIPELINE_BIND_POINT_GRAPHICS
														, frameData.PipelineLayout
														, 0
														, static_cast<uint32_t>(frameData.DescriptorSets.size())
														, frameData.DescriptorSets.data()
														, 0
														, nullptr);

			glm::mat4 const lightSpace = scene.GetLightMatrices()[scene.GetLights()[index].GetMatrixIndex()];
			context.DispatchTable.cmdPushConstants(commandBuffer
												   , frameData.PipelineLayout
												   , VK_SHADER_STAGE_VERTEX_BIT
												   , 16
												   , sizeof(glm::mat4)
												   , &lightSpace);
			for (auto& meshes = scene.GetMeshes();
				 auto& mesh: meshes)
			{
				VkDeviceSize offsets[] = { 0 };
				context.DispatchTable.cmdPushConstants(commandBuffer
													   , frameData.PipelineLayout
													   , VK_SHADER_STAGE_FRAGMENT_BIT
													   , 0
													   , sizeof(uint32_t)
													   , &mesh.GetTextureIndices().Diffuse);

				context.DispatchTable.cmdBindVertexBuffers(commandBuffer
														   , 0
														   , 1
														   , mesh.GetVertexBuffer()
														   , offsets);

				context.DispatchTable.cmdBindIndexBuffer(commandBuffer, mesh.GetIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

				context.DispatchTable.cmdDrawIndexed(commandBuffer
													 , static_cast<uint32_t>(mesh.GetIndexBuffer().GetSize() / sizeof(uint32_t))
													 , 1
													 , 0
													 , 0
													 , 0);
			}
			context.DispatchTable.cmdEndRendering(commandBuffer);
			//
			{
				vkc::Image::Transition transition{ shadowView };
				transition.NewLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				transition.SrcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
				transition.DstAccessMask = VK_ACCESS_NONE;
				transition.SrcStageMask  = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
				transition.DstStageMask  = VK_PIPELINE_STAGE_NONE;
				shadowMap.MakeTransition(context, commandBuffer, transition);
			}
		}
	}
}

#endif //VULKANRESEARCH_SHADOW_GENERATION_H
