#include "post_processing_effect.h"

#include <variant>
#include <glm/glm.hpp>
#include <iostream>

#include "pipeline.h"
#include "pipeline_layout.h"
#include "descriptor_set.h"
#include "descriptor_set_layout.h"
#include "image.h"
#include "helper.h"
#include "pingpong_render_target.h"
#include "spirv_reflect.h"
#include "imgui.h"

void PostProcessingEffect::InitializePushConstant
(std::unordered_map<std::string, std::string>& defaultValues, SpvReflectBlockVariable const& member, ShaderVariable& variable)
{
	std::variant<uint32_t, int, float, glm::vec2, glm::vec3, glm::vec4> defaultValue;
	if (variable.Type & SPV_REFLECT_TYPE_FLAG_BOOL) // never happens, bools reflected as ints
	{
		std::cout << "Found bool!" << std::endl;
	}
	else if (variable.Type & SPV_REFLECT_TYPE_FLAG_INT)
	{
		// TODO: integer vector support
		if (variable.Name.substr(0, 2) == "b_") // enforce naming convention as bools cannot be reflected properly
		{
			std::cout << "Found bool!" << std::endl;
			bool b;
			std::istringstream(defaultValues[variable.Name]) >> std::boolalpha >> b;
			defaultValue = static_cast<VkBool32>(b);
		}
		else
		{
			std::cout << "Found int!" << std::endl;
			if (variable.IsSigned)
				defaultValue = std::stoi(defaultValues[variable.Name]);
			else
				defaultValue = static_cast<uint32_t>(std::stoul(defaultValues[variable.Name]));
		}
	}
	else if (variable.Type & SPV_REFLECT_TYPE_FLAG_FLOAT)
	{
		bool const isVector{ static_cast<bool>(variable.Type & SPV_REFLECT_TYPE_FLAG_VECTOR) };
		std::cout << "Found float";
		if (isVector)
			std::cout << " vector of " + std::to_string(member.type_description->traits.numeric.vector.component_count) + " components";
		std::cout << '!' << std::endl;

		if (isVector)
		{
			switch (member.type_description->traits.numeric.vector.component_count)
			{
			case 2:
			{
				glm::vec2 temp{};
				char      delim;
				std::istringstream(defaultValues[variable.Name]) >> temp.x >> delim >> temp.y;
				defaultValue = temp;
				break;
			}
			case 3:
			{
				glm::vec3 temp{};
				char      delim;
				std::istringstream(defaultValues[variable.Name]) >> temp.x >> delim >> temp.y >> delim >> temp.z;
				defaultValue = temp;
				break;
			}
			case 4:
			{
				glm::vec4 temp{};
				char      delim;
				std::istringstream(defaultValues[variable.Name]) >> temp.x >> delim >> temp.y >> delim >> temp.z >> delim >> temp.w;
				defaultValue = temp;
				break;
			}
			default:
				break;
			}
		}
		else
			defaultValue = std::stof(defaultValues[variable.Name]);
	}

	std::visit([&variable](auto& value)
			   {
				   std::memcpy(variable.DataAddress, &value, sizeof(value));
			   }
			   , defaultValue);
}

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
		std::unordered_map<std::string, std::string> defaultValues;
		std::filesystem::path configPath{ std::filesystem::path{ "data/configs/" } / (shaderPath.stem().string() + ".ini") };
		if (std::ifstream file{ configPath })
		{
			std::string line;
			std::getline(file, line);
			assert(line == "[Config]");
			for (std::string varName; std::getline(file, varName, '=');)
			{
				std::string value;
				std::getline(file, value);
				defaultValues[varName] = value;
			}
		}
		else
			throw std::runtime_error("Can't read shader config " + m_Name);

		std::vector      effectCode = help::ReadFile(shaderPath.string());
		vkc::ShaderStage quad{ context, help::ReadFile("shaders/quad.spv"), VK_SHADER_STAGE_VERTEX_BIT };
		vkc::ShaderStage effect{ context, effectCode, VK_SHADER_STAGE_FRAGMENT_BIT };

		SpvReflectShaderModule shaderModule;
		spvReflectCreateShaderModule2(SPV_REFLECT_MODULE_FLAG_NONE, effectCode.size(), effectCode.data(), &shaderModule);

		uint32_t blockCount{};
		spvReflectEnumeratePushConstantBlocks(&shaderModule, &blockCount, nullptr);
		std::vector<SpvReflectBlockVariable*> blockVariables;
		blockVariables.resize(blockCount);
		spvReflectEnumeratePushConstantBlocks(&shaderModule, &blockCount, blockVariables.data());
		assert(blockCount == 1 && "only 1 push constant block is supported");
		for (auto& blockVariable: blockVariables)
		{
			m_PushConstants.resize(blockVariable->size);
			m_ShaderVariables.reserve(blockVariable->member_count);
			// first 2 expected to be time and array index
			for (uint32_t memberIndex{ 2 }; memberIndex < blockVariable->member_count; ++memberIndex)
			{
				auto& member   = blockVariable->members[memberIndex];
				auto& variable = m_ShaderVariables.emplace_back(member.name
																, member.type_description->type_flags
																, member.size
																, blockVariable->type_description->traits.numeric.scalar.signedness
																, &m_PushConstants[member.offset]);
				InitializePushConstant(defaultValues, member, variable);
			}
		}

		spvReflectDestroyShaderModule(&shaderModule);
		//
		{
			vkc::PipelineLayoutBuilder builder{ context };

			m_PipelineLayout = std::make_unique<vkc::PipelineLayout>(builder
																	 .AddDescriptorSetLayout(descriptorData.GlobalSetLayout)
																	 .AddDescriptorSetLayout(descriptorData.GBufferSetLayout)
																	 .AddPushConstant(VK_SHADER_STAGE_FRAGMENT_BIT
																					  , 0
																					  , static_cast<uint32_t>(m_PushConstants.size()
																						  * sizeof(m_PushConstants[0])))
																	 .Build());
		}

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
}

void PostProcessingEffect::Render
(
	vkc::Context& context, vkc::CommandBuffer& commandBuffer, RenderData& renderData, uint32_t frameIndex, bool renderToSwapchain
)
{
	VkDebugUtilsLabelEXT debugLabel{};
	debugLabel.sType      = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
	debugLabel.pLabelName = m_Name.c_str();
	static float constexpr color[4]{ .23f, 1.f, .65f, 1.f };
	for (size_t index{}; index < std::size(debugLabel.color); ++index)
		debugLabel.color[index] = color[index];
	context.DispatchTable.cmdBeginDebugUtilsLabelEXT(commandBuffer, &debugLabel);

	auto     [lastImage, lastView] = renderData.PingPongTarget.AcquireLastRenderedToTarget();
	uint32_t lastRenderedToIndex{ renderData.PingPongTarget.GetCurrentImageIndex() };

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
		uint64_t time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).
			count();
		std::memcpy(m_PushConstants.data(), &time, sizeof(time));
		std::memcpy(std::next(m_PushConstants.data(), sizeof(time)), &lastRenderedToIndex, sizeof(lastRenderedToIndex));
		context.DispatchTable.cmdPushConstants(commandBuffer
											   , *m_PipelineLayout
											   , VK_SHADER_STAGE_FRAGMENT_BIT
											   , 0
											   , static_cast<uint32_t>(m_PushConstants.size() * sizeof(m_PushConstants[0]))
											   , m_PushConstants.data());

		context.DispatchTable.cmdDraw(commandBuffer, 3, 1, 0, 0);
	}
	context.DispatchTable.cmdEndRendering(commandBuffer);
	context.DispatchTable.cmdEndDebugUtilsLabelEXT(commandBuffer);
}

void PostProcessingEffect::DrawImGUI()
{
	if (ImGui::CollapsingHeader(m_Name.c_str()))
	{
		uint32_t objectID{};
		ImGui::PushID((std::stringstream(m_Name) << m_ID << objectID++).str().c_str());
		bool const wasEnabled{ m_Enabled };
		ImGui::Checkbox("Enabled", &m_Enabled);
		ImGui::PopID();

		if (wasEnabled != m_Enabled)
			OnToggle.Execute(*this);
		for (auto& [Name, Type, Size, IsSigned, DataAddress]: m_ShaderVariables)
		{
			ImGui::PushID((std::stringstream(m_Name) << m_ID << objectID++).str().c_str());
			if (Type & SPV_REFLECT_TYPE_FLAG_INT)
			{
				if (Name.substr(0, 2) == "b_")
				{
					auto const data{ reinterpret_cast<VkBool32*>(DataAddress) };
					bool       temp{ static_cast<bool>(*data) };
					ImGui::Checkbox(Name.c_str(), &temp);
					*data = static_cast<VkBool32>(temp);
				}
				else if (IsSigned)
					ImGui::SliderInt(Name.c_str(), reinterpret_cast<int*>(DataAddress), -255, 255);
				else
				{
					auto const data{ reinterpret_cast<uint32_t*>(DataAddress) };
					int        temp{ static_cast<int>(*data) };
					ImGui::SliderInt(Name.c_str(), &temp, 0, 255);
					*data = static_cast<uint32_t>(temp);
				}
			}
			else if (Type & SPV_REFLECT_TYPE_FLAG_FLOAT)
			{
				if (Type & SPV_REFLECT_TYPE_FLAG_VECTOR)
				{
					uint32_t const elementCount{ static_cast<uint32_t>(Size / sizeof(float)) };
					assert(1 < elementCount && elementCount <= 4);
					switch (elementCount)
					{
					case 2:
						ImGui::SliderFloat2(Name.c_str(), reinterpret_cast<float*>(DataAddress), .0f, 1.0f);
						break;
					case 3:
						ImGui::SliderFloat3(Name.c_str(), reinterpret_cast<float*>(DataAddress), .0f, 1.0f);
						break;
					case 4:
						ImGui::SliderFloat4(Name.c_str(), reinterpret_cast<float*>(DataAddress), .0f, 1.0f);
						break;
					default:
						break;
					}
				}
				else
					ImGui::SliderFloat(Name.c_str(), reinterpret_cast<float*>(DataAddress), .0f, 1.f);
			}
			ImGui::PopID();
		}
	}
}
