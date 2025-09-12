#include "app.h"

#include <iostream>

#include "command_pool.h"
#include "datatypes.h"
#include "helper.h"
#include "image.h"
#include "shadow_generation.h"
#include "shader_stage.h"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"

#include <span>
#include <chrono>
#include <ranges>

#include "scene.h"

#include "image_view.h"

App::App(int width, int height)
{
	auto const start = std::chrono::steady_clock::now();
	double     initDuration{};
	//
	{
		m_Camera = std::make_unique<Camera>(glm::vec3(.0f, .0f, .0f)
											, 45.f
											, static_cast<float>(width) / height // NOLINT(*-narrowing-conversions)
											, .01f
											, 50.f);
		CreateWindow(width, height);
		CreateInstance();
		CreateSurface();
		CreateDevice();
		CreateSwapchain();
		m_Context.DeletionQueue.Push([this]
		{
			std::vector<VkImageView> views;
			views.reserve(m_SwapchainImageViews.size());
			for (uint32_t index{}; index < m_SwapchainImageViews.size(); ++index)
				views.emplace_back(m_SwapchainImageViews[index]);
			m_Context.Swapchain.destroy_image_views(views);
			vkb::destroy_swapchain(m_Context.Swapchain);
		});
		CreateCmdPool();
		auto const end = std::chrono::steady_clock::now();
		initDuration   = std::chrono::duration<double>(end - start).count();
	}
	//
	{
		auto const localStart = std::chrono::steady_clock::now();
		CreateScene();
		auto const end  = std::chrono::steady_clock::now();
		m_CPUTimings[2] = Timing{ "Scene load", std::chrono::duration<double>(end - localStart).count() };
	}
	//
	{
		auto const localStart = std::chrono::steady_clock::now();
		CreateResources();
		CreateDescriptorSetLayouts();
		CreateGraphicsPipeline();
		CreateSyncObjects();
		CreateDescriptorPool();
		CreateDescriptorSets();
		auto const end = std::chrono::steady_clock::now();
		initDuration += std::chrono::duration<double>(end - localStart).count();
	}
	auto const end  = std::chrono::steady_clock::now();
	m_CPUTimings[1] = Timing{ "Vulkan init", initDuration };
	m_CPUTimings[3] = Timing{ "Total init", std::chrono::duration<double>(end - start).count() };
	InitImGUI();
	GenerateShadowMaps();
}

App::~App() = default;

void App::Run()
{
	// main loop
	while (!glfwWindowShouldClose(m_Context.Window))
	{
		auto const start = std::chrono::steady_clock::now();
		m_QueryPool->GetResults(m_Context, m_GPUTimings);

		glfwPollEvents();
		m_Context.DispatchTable.waitForFences(1, &m_InFlightFences[m_CurrentFrame], VK_TRUE, UINT64_MAX);

		world_time::Tick();
		m_Camera->Update(m_Context.Window);

		ModelViewProj const mvp{ glm::mat4{ 1 }, m_Camera->CalculateViewMatrix(), m_Camera->GetProjection() };

		m_MVPUBOs[m_CurrentFrame].UpdateData(mvp);
		uint32_t imageIndex{};
		if (auto const result = m_Context.DispatchTable.acquireNextImageKHR(m_Context.Swapchain
																			, UINT64_MAX
																			, m_ImageAvailableSemaphores[m_CurrentFrame]
																			, VK_NULL_HANDLE
																			, &imageIndex);
			result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			RecreateSwapchain();
			return;
		}

		vkc::CommandBuffer& commandBuffer = m_CommandPool->AllocateCommandBuffer(m_Context);
		m_Context.DispatchTable.resetFences(1, &m_InFlightFences[m_CurrentFrame]);

		using namespace std::placeholders;
		commandBuffer.Begin(m_Context);
		m_QueryPool->Reset(commandBuffer);
		m_QueryPool->RecordWholePipe(commandBuffer
									 , "Total GPU frametime"
									 , -1
									 , [this, &commandBuffer, imageIndex]
									 {
										 RecordCommandBuffer(commandBuffer, imageIndex);
									 });
		commandBuffer.End(m_Context);

		Submit(commandBuffer);

		Present(imageIndex);

		++m_CurrentFrame;
		m_CurrentFrame %= m_FramesInFlight;
		auto const end  = std::chrono::steady_clock::now();
		m_CPUTimings[5] = Timing{ "CPU frame time", std::chrono::duration<double>(end - start).count() };
	}

	if (m_Context.DispatchTable.deviceWaitIdle() != VK_SUCCESS)
		throw std::runtime_error("Failed to wait for the device");

	End();
}

void App::InitImGUI() const
{
	ImGui::CreateContext();
	ImGui_ImplGlfw_InitForVulkan(m_Context.Window, true);
	ImGui_ImplVulkan_InitInfo initInfo{};
	initInfo.ApiVersion          = VK_API_VERSION_1_3;
	initInfo.Instance            = m_Context.Instance;
	initInfo.PhysicalDevice      = m_PhysicalDevice.physical_device;
	initInfo.Device              = m_Context.Device;
	initInfo.QueueFamily         = m_Context.Device.get_queue_index(vkb::QueueType::graphics).value();
	initInfo.Queue               = m_Context.GraphicsQueue;
	initInfo.DescriptorPool      = *m_DescPool;
	initInfo.MinImageCount       = m_FramesInFlight;
	initInfo.ImageCount          = m_FramesInFlight;
	initInfo.UseDynamicRendering = true;
	VkPipelineRenderingCreateInfo pipelineRendering{};
	pipelineRendering.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	pipelineRendering.colorAttachmentCount    = 1;
	pipelineRendering.pColorAttachmentFormats = &m_Context.Swapchain.image_format;
	pipelineRendering.depthAttachmentFormat   = m_DepthFormat;
	pipelineRendering.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;
	initInfo.PipelineRenderingCreateInfo      = pipelineRendering;
	ImGui_ImplVulkan_Init(&initInfo);
}

void App::DrawImGui()
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 20));
	ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 4.f);
	ImGui::Begin("Timing information", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize);
	if (ImGui::CollapsingHeader("CPU Timings", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (ImGui::BeginTable("CPU_Timing_Table", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
		{
			ImGui::TableSetupColumn("Stage");
			ImGui::TableSetupColumn("Duration (s)", ImGuiTableColumnFlags_WidthFixed, 120.0f);
			ImGui::TableHeadersRow();

			for (auto const& timing: m_CPUTimings | std::views::values)
			{
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::TextUnformatted(timing.GetLabel().data());

				ImGui::TableSetColumnIndex(1);
				ImGui::Text("%.4f", timing.GetDuration());
			}
			ImGui::EndTable();
		}
	}
	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();
	if (ImGui::CollapsingHeader("GPU Timings", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (ImGui::BeginTable("GPU_Timing_Table", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
		{
			ImGui::TableSetupColumn("Stage");
			ImGui::TableSetupColumn("Duration (ms)", ImGuiTableColumnFlags_WidthFixed, 120.0f);
			ImGui::TableHeadersRow();

			for (auto const& timing: m_GPUTimings | std::views::values)
			{
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::TextUnformatted(timing.GetLabel().data());

				ImGui::TableSetColumnIndex(1);

				double const duration = timing.GetDuration();

				float constexpr minMs = .1f;
				float constexpr maxMs = .5f;

				auto t = static_cast<float>((duration - minMs) / (maxMs - minMs));
				t      = std::clamp(t, 0.0f, 1.0f);

				ImVec4 constexpr green(0.3f, 1.0f, 0.3f, 1.0f);
				ImVec4 constexpr red(1.0f, 0.3f, 0.3f, 1.0f);

				ImVec4 color;
				color.x = green.x + t * (red.x - green.x);
				color.y = green.y + t * (red.y - green.y);
				color.z = green.z + t * (red.z - green.z);
				color.w = 1.0f;

				ImGui::TextColored(color, "%.4f", duration);
			}
			ImGui::EndTable();
		}
	}
	ImGui::End();
	ImGui::PopStyleVar();
	ImGui::PopStyleVar();
}

void App::CreateWindow(int width, int height)
{
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
	m_Context.Window = glfwCreateWindow(width, height, "Base window", nullptr, nullptr);
	m_Context.DeletionQueue.Push([this]
	{
		glfwDestroyWindow(m_Context.Window);
		glfwTerminate();
	});
}

void App::CreateInstance()
{
	vkb::InstanceBuilder instanceBuilder{};
	auto const           instanceResult = instanceBuilder
								.use_default_debug_messenger()
								.require_api_version(1, 3)
								.request_validation_layers()
								.build();
	if (!instanceResult)
		throw std::runtime_error("failed to create instance");

	m_Context.Instance              = instanceResult.value();
	m_Context.InstanceDispatchTable = m_Context.Instance.make_table(); // instance function table

	m_Context.DeletionQueue.Push([this]
	{
		vkb::destroy_instance(m_Context.Instance);
	});
}

void App::CreateSurface()
{
	// create surface
	if (VkResult const result = glfwCreateWindowSurface(m_Context.Instance, m_Context.Window, nullptr, &m_Context.Surface);
		result != VK_SUCCESS)
	{
		char const* errorMessage;
		if (glfwGetError(&errorMessage))
			throw std::runtime_error(errorMessage);
	}
	m_Context.DeletionQueue.Push([this]
	{
		m_Context.InstanceDispatchTable.destroySurfaceKHR(m_Context.Surface, nullptr);
	});
}

void App::CreateDevice()
{
	VkPhysicalDeviceVulkan11Features features11{};
	features11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
	VkPhysicalDeviceVulkan12Features features12{};
	features12.sType                                        = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	features12.runtimeDescriptorArray                       = VK_TRUE;
	features12.descriptorBindingPartiallyBound              = VK_TRUE;
	features12.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
	features12.descriptorBindingVariableDescriptorCount     = VK_TRUE;
	features12.descriptorIndexing                           = VK_TRUE;
	features12.shaderSampledImageArrayNonUniformIndexing    = VK_TRUE;
	VkPhysicalDeviceVulkan13Features features13{};
	features13.sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	features13.dynamicRendering = VK_TRUE;
	features13.synchronization2 = VK_TRUE;

	std::vector extensions{ VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME, VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME };

	vkb::PhysicalDeviceSelector selector{ m_Context.Instance };
	auto const                  physicalDeviceResult = selector
									  .prefer_gpu_device_type()
									  // .add_required_extension_features(features)
									  .add_required_extension_features(features11)
									  .add_required_extension_features(features12)
									  .add_required_extension_features(features13)
									  .add_required_extensions(extensions)
									  .set_minimum_version(1, 3)
									  .set_surface(m_Context.Surface)
									  .select();
	if (!physicalDeviceResult)
		throw std::runtime_error("failed to create physical device");

	m_DepthFormat = help::FindSupportedFormat(physicalDeviceResult.value()
											  , { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT }
											  , VK_IMAGE_TILING_OPTIMAL
											  , VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

	m_PhysicalDevice = physicalDeviceResult.value();

	auto const deviceResult = vkb::DeviceBuilder{ physicalDeviceResult.value() }.build();
	if (!deviceResult)
		throw std::runtime_error("failed to create device");

	m_Context.Device        = deviceResult.value();
	m_Context.DispatchTable = m_Context.Device.make_table();
	//
	{
		auto const result = m_Context.Device.get_queue(vkb::QueueType::graphics);
		if (!result)
			throw std::runtime_error("Failed to get a graphics queue");
		m_Context.GraphicsQueue = result.value();
	}
	//
	{
		auto const result = m_Context.Device.get_queue(vkb::QueueType::present);
		if (!result)
			throw std::runtime_error("Failed to get a presenting queue");
		m_Context.PresentQueue = result.value();
	}
	m_Context.DeletionQueue.Push([this]
	{
		vkb::destroy_device(m_Context.Device);
	});

	VmaAllocatorCreateInfo allocatorInfo{};
	allocatorInfo.device           = m_Context.Device;
	allocatorInfo.instance         = m_Context.Instance;
	allocatorInfo.physicalDevice   = physicalDeviceResult.value();
	allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
	vmaCreateAllocator(&allocatorInfo, &m_Context.Allocator);
	m_QueryPool = std::make_unique<TimingQueryPool>(m_Context, m_PhysicalDevice.properties.limits.timestampPeriod);
	m_Context.DeletionQueue.Push([this]
	{
		m_QueryPool->Destroy(m_Context);
		vmaDestroyAllocator(m_Context.Allocator);
	});
}

void App::CreateSwapchain()
{
	vkb::SwapchainBuilder builder{ m_Context.Device };
	auto const            swapchainResult = builder
								 .set_old_swapchain(m_Context.Swapchain)
								 .build();
	if (!swapchainResult)
		throw std::runtime_error("Failed to create swapchain" + swapchainResult.error().message() +
								 " " + std::to_string(swapchainResult.vk_result()));

	vkb::destroy_swapchain(m_Context.Swapchain);
	m_Context.Swapchain = swapchainResult.value();

	vkc::Image::ConvertFromSwapchainVkImages(m_Context, m_SwapchainImages);
	vkc::ImageView::ConvertFromSwapchainVkImageViews(m_Context, m_SwapchainImageViews);

	m_FramesInFlight = m_Context.Swapchain.image_count;
}

void App::CreateSyncObjects()
{
	VkSemaphoreCreateInfo semaphoreCreateInfo{};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	VkFenceCreateInfo fenceCreateInfo{};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	m_ImageAvailableSemaphores.resize(m_FramesInFlight);
	m_RenderFinishedSemaphores.resize(m_FramesInFlight);
	m_InFlightFences.resize(m_FramesInFlight);

	for (size_t index{}; index < m_FramesInFlight; ++index)
	{
		if (m_Context.DispatchTable.createSemaphore(&semaphoreCreateInfo, nullptr, &m_ImageAvailableSemaphores[index]) != VK_SUCCESS
			||
			m_Context.DispatchTable.createSemaphore(&semaphoreCreateInfo, nullptr, &m_RenderFinishedSemaphores[index]) != VK_SUCCESS
			||
			m_Context.DispatchTable.createFence(&fenceCreateInfo, nullptr, &m_InFlightFences[index]) != VK_SUCCESS)
			throw std::runtime_error("Failed to create semaphores");
		m_Context.DeletionQueue.Push([index, this]
		{
			m_Context.DispatchTable.destroySemaphore(m_ImageAvailableSemaphores[index], nullptr);
			m_Context.DispatchTable.destroySemaphore(m_RenderFinishedSemaphores[index], nullptr);
			m_Context.DispatchTable.destroyFence(m_InFlightFences[index], nullptr);
		});
	}
}

void App::CreateDescriptorSetLayouts()
{
	//
	{
		vkc::DescriptorSetLayoutBuilder builder{ m_Context };
		vkc::DescriptorSetLayout        layout = builder
										  .AddBinding(0
													  , VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
													  , VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
										  .AddBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
										  .AddBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
										  .AddBinding(3, VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
										  .Build();

		m_FrameDescSetLayout = std::make_unique<vkc::DescriptorSetLayout>(std::move(layout));
	}
	//
	{
		vkc::DescriptorSetLayoutBuilder builder{ m_Context };
		vkc::DescriptorSetLayout        layout = builder
										  .AddBinding(0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT)
										  .AddBinding(1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT)
										  .AddBinding(2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT)
										  .AddBinding(3, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT, 2)
										  .Build();

		m_GbufferDescSetLayout = std::make_unique<vkc::DescriptorSetLayout>(std::move(layout));
	}
	//
	{
		VkSamplerCreateInfo samplerCreateInfo{};
		samplerCreateInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerCreateInfo.minFilter               = VK_FILTER_LINEAR;
		samplerCreateInfo.magFilter               = VK_FILTER_LINEAR;
		samplerCreateInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
		samplerCreateInfo.compareEnable           = VK_FALSE;

		m_Context.DispatchTable.createSampler(&samplerCreateInfo, nullptr, &m_TextureSampler);

		samplerCreateInfo.addressModeU  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCreateInfo.addressModeV  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCreateInfo.addressModeW  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCreateInfo.compareEnable = VK_TRUE;
		samplerCreateInfo.compareOp     = VK_COMPARE_OP_LESS;
		m_Context.DispatchTable.createSampler(&samplerCreateInfo, nullptr, &m_ShadowSampler);

		m_Context.DeletionQueue.Push([this]
		{
			m_Context.DispatchTable.destroySampler(m_TextureSampler, nullptr);
			m_Context.DispatchTable.destroySampler(m_ShadowSampler, nullptr);
		});

		uint32_t constexpr variableTextureCount{ 1024 };

		vkc::DescriptorSetLayoutBuilder builder{ m_Context };
		vkc::DescriptorSetLayout        layout = builder
										  .AddBinding(0, VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
										  .AddBinding(1
													  , VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE
													  , VK_SHADER_STAGE_FRAGMENT_BIT
													  , variableTextureCount
													  , VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
														| VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT)
										  .Build();
		m_GlobalDescSetLayout = std::make_unique<vkc::DescriptorSetLayout>(std::move(layout));
		help::NameObject(m_Context
						 , reinterpret_cast<uint64_t>(static_cast<VkDescriptorSetLayout>(*m_GlobalDescSetLayout))
						 , VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT
						 , "global descriptor set layout");
	}
}

void App::GenerateShadowMaps()
{
	//
	{
		std::vector<vkc::Image>     directionalShadowMaps;
		std::vector<vkc::ImageView> directionalShadowMapViews;

		std::vector<vkc::Image>                  pointShadowMaps;
		std::vector<std::vector<vkc::ImageView>> pointShadowMapViews;

		constexpr VkExtent2D shadowMapResolution{ 2048, 2048 };
		vkc::ImageBuilder    builder{ m_Context };
		builder
			.SetAspectFlags(m_DepthImage->GetAspect())
			.SetFormat(m_DepthImage->GetFormat())
			.SetType(VK_IMAGE_TYPE_2D)
			.SetExtent(shadowMapResolution);

		if (m_Scene->GetDirectionalLightCount() > 0)
			for (uint32_t index{}; index < m_Scene->GetDirectionalLightCount(); ++index)
			{
				directionalShadowMaps.emplace_back(builder.Build(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT));
				directionalShadowMapViews.emplace_back(directionalShadowMaps[index].CreateView(m_Context, VK_IMAGE_VIEW_TYPE_2D));

				help::NameObject(m_Context
								 , reinterpret_cast<uint64_t>(static_cast<VkImage>(directionalShadowMaps[index]))
								 , VK_OBJECT_TYPE_IMAGE
								 , "Directional shadow map");
				help::NameObject(m_Context
								 , reinterpret_cast<uint64_t>(static_cast<VkImageView>(directionalShadowMapViews[index]))
								 , VK_OBJECT_TYPE_IMAGE_VIEW
								 , "Directional shadow map view");
			}

		builder
			.SetFlags(VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT)
			.SetLayers(6);
		if (m_Scene->GetPointLightCount() > 0)
			for (uint32_t index{}; index < m_Scene->GetPointLightCount(); ++index)
			{
				auto& map = pointShadowMaps.
					emplace_back(builder.Build(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT));
				auto& views = pointShadowMapViews.emplace_back();
				for (int viewIndex{}; viewIndex < 6; ++viewIndex)
					views.emplace_back(map.CreateView(m_Context, VK_IMAGE_VIEW_TYPE_2D, viewIndex, 1, 0, 1, false));

				help::NameObject(m_Context
								 , reinterpret_cast<uint64_t>(static_cast<VkImage>(map))
								 , VK_OBJECT_TYPE_IMAGE
								 , "point shadow map");
			}

		auto [directionalPipelineLayout, directionalPipeline] = shadow::CreatePipelineForDirectionalShadows(m_Context
				 , *m_GlobalDescSetLayout
				 , m_DepthFormat
				 , shadowMapResolution);
		auto [pointPipelineLayout, pointPipeline] = shadow::CreatePipelineForPointShadows(m_Context
																						  , *m_GlobalDescSetLayout
																						  , m_DepthFormat
																						  , shadowMapResolution);

		vkc::CommandBuffer& commandBuffer = m_CommandPool->AllocateCommandBuffer(m_Context);
		commandBuffer.Begin(m_Context, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
		m_QueryPool->Reset(commandBuffer);
		std::string const label{ "Shadow generation" };
		m_QueryPool->WriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, label, 0);
		VkDescriptorSet descSets[]{ m_GlobalDescriptorSets[m_CurrentFrame] };

		FrameData const pointLightData{
			.PipelineLayout = &pointPipelineLayout
			, .Pipeline = &pointPipeline
			, .DescriptorSetLayouts = { m_GlobalDescSetLayout.get(), 1 }
			, .DescriptorSets = { descSets }
		};

		FrameData const directionalLightData{
			.PipelineLayout = &directionalPipelineLayout
			, .Pipeline = &directionalPipeline
			, .DescriptorSetLayouts = { m_GlobalDescSetLayout.get(), 1 }
			, .DescriptorSets = { descSets }
		};
		shadow::RecordPointShadowsGeneration(m_Context
											 , commandBuffer
											 , *m_Scene
											 , pointShadowMaps
											 , pointShadowMapViews
											 , pointLightData);

		shadow::RecordDirectionalShadowsGeneration(m_Context
												   , commandBuffer
												   , *m_Scene
												   , directionalShadowMaps
												   , directionalShadowMapViews
												   , directionalLightData);
		m_QueryPool->WriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, label, 0);

		commandBuffer.End(m_Context);
		commandBuffer.Submit(m_Context, m_Context.GraphicsQueue, {}, {});
		if (m_Context.DispatchTable.waitForFences(1, &commandBuffer.GetFence(), VK_TRUE, UINT64_MAX) != VK_SUCCESS)
			throw std::runtime_error("failed to wait for command buffer fence");

		for (auto& views: pointShadowMapViews)
			for (auto& view: views)
				view.Destroy(m_Context);

		for (uint32_t index{}; index < directionalShadowMaps.size(); ++index)
		{
			uint32_t const textureIndex = m_Scene->AddTextureToPool(std::move(directionalShadowMaps[index])
																	, std::move(directionalShadowMapViews[index]));
			m_Scene->GetLights()[index].LinkShadowMapIndex(textureIndex);
		}

		for (uint32_t index{ m_Scene->GetDirectionalLightCount() }; index < m_Scene->GetLights().size(); ++index)
		{
			auto& map  = pointShadowMaps[index - m_Scene->GetDirectionalLightCount()];
			auto  view = map.CreateView(m_Context, VK_IMAGE_VIEW_TYPE_CUBE, 0, 6);

			uint32_t const textureIndex = m_Scene->AddTextureToPool(std::move(map)
																	, std::move(view));
			m_Scene->GetLights()[index].LinkShadowMapIndex(textureIndex);
		}
		directionalShadowMaps.clear();
		directionalShadowMapViews.clear();

		directionalPipeline.Destroy(m_Context);
		directionalPipelineLayout.Destroy(m_Context);
		pointPipeline.Destroy(m_Context);
		pointPipelineLayout.Destroy(m_Context);
		// update descriptor texture array with newly created shadow maps
		{
			auto const& textures     = m_Scene->GetTextureImages();
			auto const& textureViews = m_Scene->GetTextureImageViews();

			std::vector<VkDescriptorImageInfo> imageInfos;
			imageInfos.reserve(textures.size());

			const size_t textureCountWithoutShadowMaps = textures.size() - m_Scene->GetLights().size();

			for (auto [image, view]{
					 std::make_pair(textures.begin() + textureCountWithoutShadowMaps, textureViews.begin() + textureCountWithoutShadowMaps)
				 };
				 image != textures.end() && view != textureViews.end();
				 ++image, ++view)
			{
				imageInfos.emplace_back(VK_NULL_HANDLE, *view, image->GetLayout());
			}

			for (uint32_t index{}; index < m_GlobalDescriptorSets.size(); ++index)
			{
				m_GlobalDescriptorSets[index]
					.AddWriteDescriptor(imageInfos
										, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE
										, 1
										, static_cast<uint32_t>(textureCountWithoutShadowMaps))
					.Update(m_Context);
			}
		}
	}

	for (auto& ssbo: m_LightMatricesSSBOs)
		ssbo.UpdateData(m_Scene->GetLightMatrices());
	for (auto& ssbo: m_LightSSBOs)
		ssbo.UpdateData(m_Scene->GetLights());
}

void App::CreateDescriptorPool()
{
	vkc::DescriptorPoolBuilder builder{ m_Context };
	vkc::DescriptorPool        pool = builder
							   .SetFlags(VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT)
							   .AddPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, m_FramesInFlight) // mvp
							   .AddPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, m_FramesInFlight) // light data
							   .AddPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, m_FramesInFlight) // light data
							   .AddPoolSize(VK_DESCRIPTOR_TYPE_SAMPLER, m_FramesInFlight)        // sampler
							   .AddPoolSize(VK_DESCRIPTOR_TYPE_SAMPLER, m_FramesInFlight)        // shadow sampler
							   .AddPoolSize(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, m_FramesInFlight)  // textures
							   .AddPoolSize(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, m_FramesInFlight)  // albedo
							   .AddPoolSize(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, m_FramesInFlight)  // normals and material
							   .AddPoolSize(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, m_FramesInFlight)  // depth
							   .AddPoolSize(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, m_FramesInFlight)  // shadow maps
							   .AddPoolSize(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, m_FramesInFlight)  // hdri
							   .AddPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)        // imgui
							   .Build(4 * m_FramesInFlight);

	m_DescPool = std::make_unique<vkc::DescriptorPool>(std::move(pool));
}

void App::UpdateGbufferDescriptor()
{
	VkDescriptorImageInfo albedoInfo{};
	albedoInfo.sampler     = VK_NULL_HANDLE;
	albedoInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	albedoInfo.imageView   = *m_AlbedoView;

	VkDescriptorImageInfo normalsInfo{};
	normalsInfo.sampler     = VK_NULL_HANDLE;
	normalsInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	normalsInfo.imageView   = *m_MaterialView;

	VkDescriptorImageInfo depthInfo{};
	depthInfo.sampler     = VK_NULL_HANDLE;
	depthInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	depthInfo.imageView   = *m_DepthImageView;

	auto const hdriViews = m_HDRIRenderTarget->GetViews();

	VkDescriptorImageInfo HDRIInfo[2]{};
	for (int index{}; index < 2; ++index)
	{
		HDRIInfo[index].sampler     = VK_NULL_HANDLE;
		HDRIInfo[index].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		HDRIInfo[index].imageView   = hdriViews[index];
	}

	for (uint32_t index{}; index < m_FramesInFlight; ++index)
	{
		m_GbufferDescriptorSets[index]
			.AddWriteDescriptor({ &albedoInfo, 1 }, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 0, 0)
			.AddWriteDescriptor({ &normalsInfo, 1 }, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, 0)
			.AddWriteDescriptor({ &depthInfo, 1 }, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 2, 0)
			.AddWriteDescriptor(HDRIInfo, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 3, 0)
			.Update(m_Context);
	}
}

void App::CreateDescriptorSets()
{
	//
	{
		std::vector<VkDescriptorSetLayout> layouts(m_FramesInFlight, *m_FrameDescSetLayout);

		vkc::DescriptorSetBuilder const builder{ m_Context };
		m_FrameDescriptorSets = builder
			.Build(*m_DescPool, layouts);

		for (uint32_t index{}; index < m_FramesInFlight; ++index)
		{
			VkDescriptorBufferInfo bufferInfo{};
			bufferInfo.buffer = m_MVPUBOs[index];
			bufferInfo.range  = VK_WHOLE_SIZE;
			bufferInfo.offset = 0;

			VkDescriptorBufferInfo lightInfo{};
			lightInfo.buffer = m_LightSSBOs[index];
			lightInfo.range  = VK_WHOLE_SIZE;
			lightInfo.offset = 0;

			VkDescriptorBufferInfo lightMatricesInfo{};
			if (!m_LightMatricesSSBOs.empty())
			{
				lightMatricesInfo.buffer = m_LightMatricesSSBOs[index];
				lightMatricesInfo.range  = VK_WHOLE_SIZE;
				lightMatricesInfo.offset = 0;
				m_FrameDescriptorSets[index]
					.AddWriteDescriptor({ &lightMatricesInfo, 1 }, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2, 0);
			}

			VkDescriptorImageInfo shadowSamplerInfo{};
			shadowSamplerInfo.sampler     = m_ShadowSampler;
			shadowSamplerInfo.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			shadowSamplerInfo.imageView   = VK_NULL_HANDLE;

			m_FrameDescriptorSets[index]
				.AddWriteDescriptor({ &bufferInfo, 1 }, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, 0)
				.AddWriteDescriptor({ &lightInfo, 1 }, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, 0)
				.AddWriteDescriptor({ &shadowSamplerInfo, 1 }, VK_DESCRIPTOR_TYPE_SAMPLER, 3, 0)
				.Update(m_Context);
		}
	}
	//
	{
		std::vector<VkDescriptorSetLayout> layouts(m_FramesInFlight, *m_GbufferDescSetLayout);

		vkc::DescriptorSetBuilder const builder{ m_Context };
		m_GbufferDescriptorSets = builder
			.Build(*m_DescPool, layouts);

		UpdateGbufferDescriptor();
	}
	//
	{
		std::vector<VkDescriptorSetLayout> layouts(m_FramesInFlight, *m_GlobalDescSetLayout);

		VkDescriptorImageInfo samplerInfo[]
		{
			{
				.sampler = m_TextureSampler
				, .imageView = VK_NULL_HANDLE
				, .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED
			}
		};

		auto const& textures     = m_Scene->GetTextureImages();
		auto const& textureViews = m_Scene->GetTextureImageViews();

		std::vector<VkDescriptorImageInfo> imageInfos;
		imageInfos.reserve(textures.size());

		for (auto [image, view]{ std::make_pair(textures.begin(), textureViews.begin()) };
			 image != textures.end() && view != textureViews.end();
			 ++image, ++view)
		{
			imageInfos.emplace_back(VK_NULL_HANDLE, *view, image->GetLayout());
		}

		uint32_t const actualSize = static_cast<uint32_t>(m_Scene->GetTextureImages().size() + m_Scene->GetLights().size());

		std::vector counts(m_FramesInFlight, actualSize);

		vkc::DescriptorSetBuilder builder{ m_Context };
		m_GlobalDescriptorSets = builder
								 .AddVariableDescriptorCount(counts)
								 .Build(*m_DescPool, layouts);

		for (auto& descriptor: m_GlobalDescriptorSets)
			descriptor
				.AddWriteDescriptor(samplerInfo, VK_DESCRIPTOR_TYPE_SAMPLER, 0, 0)
				.AddWriteDescriptor(imageInfos
									, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE
									, 1
									, 0)
				.Update(m_Context);
	}
}

void App::CreateGraphicsPipeline()
{
	// depth prepass layout
	{
		vkc::PipelineLayoutBuilder builder{ m_Context };
		vkc::PipelineLayout        layout = builder
									 .AddDescriptorSetLayout(*m_GlobalDescSetLayout)
									 .AddDescriptorSetLayout(*m_FrameDescSetLayout)
									 .AddPushConstant(VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(TextureIndices::Diffuse))
									 .Build();
		m_DepthPrepPipelineLayout = std::make_unique<vkc::PipelineLayout>(std::move(layout));
	}
	// gbuffer gen layout
	{
		vkc::PipelineLayoutBuilder builder{ m_Context };
		vkc::PipelineLayout        layout = builder
									 .AddDescriptorSetLayout(*m_GlobalDescSetLayout)
									 .AddDescriptorSetLayout(*m_FrameDescSetLayout)
									 .AddPushConstant(VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(TextureIndices))
									 .Build();
		m_GBufferGenPipelineLayout = std::make_unique<vkc::PipelineLayout>(std::move(layout));
	}
	// lighting layout
	{
		vkc::PipelineLayoutBuilder builder{ m_Context };
		vkc::PipelineLayout        layout = builder
									 .AddDescriptorSetLayout(*m_GlobalDescSetLayout)
									 .AddDescriptorSetLayout(*m_FrameDescSetLayout)
									 .AddDescriptorSetLayout(*m_GbufferDescSetLayout)
									 .Build();
		m_LightingPipelineLayout = std::make_unique<vkc::PipelineLayout>(std::move(layout));
		VkDebugUtilsObjectNameInfoEXT debugNameInfo{};
		debugNameInfo.sType        = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
		debugNameInfo.objectType   = VK_OBJECT_TYPE_PIPELINE_LAYOUT;
		debugNameInfo.objectHandle = reinterpret_cast<uint64_t>(static_cast<VkPipelineLayout>(*m_LightingPipelineLayout));
		debugNameInfo.pObjectName  = "Pipeline Layout (lighting)";
		if (m_Context.DispatchTable.setDebugUtilsObjectNameEXT(&debugNameInfo) != VK_SUCCESS)
			throw std::runtime_error("failed to set debug object name");
	}
	// blit layout
	{
		vkc::PipelineLayoutBuilder builder{ m_Context };
		vkc::PipelineLayout        layout = builder
									 .AddPushConstant(VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(uint32_t))
									 .AddDescriptorSetLayout(*m_GlobalDescSetLayout)
									 .AddDescriptorSetLayout(*m_GbufferDescSetLayout)
									 .Build();
		m_BlitPipelineLayout = std::make_unique<vkc::PipelineLayout>(std::move(layout));
		VkDebugUtilsObjectNameInfoEXT debugNameInfo{};
		debugNameInfo.sType        = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
		debugNameInfo.objectType   = VK_OBJECT_TYPE_PIPELINE_LAYOUT;
		debugNameInfo.objectHandle = reinterpret_cast<uint64_t>(static_cast<VkPipelineLayout>(*m_BlitPipelineLayout));
		debugNameInfo.pObjectName  = "Pipeline Layout (blit)";
		if (m_Context.DispatchTable.setDebugUtilsObjectNameEXT(&debugNameInfo) != VK_SUCCESS)
			throw std::runtime_error("failed to set debug object name");
	}

	VkPipelineColorBlendAttachmentState blendAttachment{};
	blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT
									 | VK_COLOR_COMPONENT_G_BIT
									 | VK_COLOR_COMPONENT_B_BIT
									 | VK_COLOR_COMPONENT_A_BIT;

	// depth prepass pipeline
	{
		vkc::ShaderStage const vert{ m_Context, help::ReadFile("shaders/basic_transform.spv"), VK_SHADER_STAGE_VERTEX_BIT };
		vkc::ShaderStage       alphaDiscard{ m_Context, help::ReadFile("shaders/alpha_discard.spv"), VK_SHADER_STAGE_FRAGMENT_BIT };

		vkc::PipelineBuilder builder{ m_Context };
		vkc::Pipeline        pipeline = builder
								 .SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
								 .AddViewport(m_Context.Swapchain.extent)
								 .SetPolygonMode(VK_POLYGON_MODE_FILL)
								 .SetCullMode(VK_CULL_MODE_BACK_BIT)
								 .SetFrontFace(VK_FRONT_FACE_COUNTER_CLOCKWISE)
								 .SetVertexDescription(Vertex::GetBindingDescription(), Vertex::GetAttributeDescription())
								 .AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
								 .AddDynamicState(VK_DYNAMIC_STATE_SCISSOR)
								 .SetRenderingAttachments({}, m_DepthFormat, VK_FORMAT_UNDEFINED)
								 .EnableDepthTest(VK_COMPARE_OP_LESS)
								 .EnableDepthWrite()
								 .AddShaderStage(vert)
								 .AddShaderStage(alphaDiscard)
								 .Build(*m_DepthPrepPipelineLayout, true);
		m_DepthPrepPipeline = std::make_unique<vkc::Pipeline>(std::move(pipeline));
	}
	// gbuffer gen pipeline
	{
		vkc::ShaderStage const vert{ m_Context, help::ReadFile("shaders/transform_w_normals.spv"), VK_SHADER_STAGE_VERTEX_BIT };
		vkc::ShaderStage       frag{ m_Context, help::ReadFile("shaders/gbuffer_generation.spv"), VK_SHADER_STAGE_FRAGMENT_BIT };

		VkFormat colorAttachmentFormats[]{ m_AlbedoImage->GetFormat(), m_MaterialImage->GetFormat() };

		vkc::PipelineBuilder builder{ m_Context };
		vkc::Pipeline        pipeline = builder
								 .SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
								 .AddViewport(m_Context.Swapchain.extent)
								 .SetPolygonMode(VK_POLYGON_MODE_FILL)
								 .SetCullMode(VK_CULL_MODE_BACK_BIT)
								 .SetFrontFace(VK_FRONT_FACE_COUNTER_CLOCKWISE)
								 .SetVertexDescription(Vertex::GetBindingDescription(), Vertex::GetAttributeDescription())
								 .AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
								 .AddDynamicState(VK_DYNAMIC_STATE_SCISSOR)
								 .AddColorBlendAttachment(blendAttachment)
								 .AddColorBlendAttachment(blendAttachment)
								 .SetRenderingAttachments(colorAttachmentFormats, m_DepthFormat, VK_FORMAT_UNDEFINED)
								 .EnableDepthTest(VK_COMPARE_OP_EQUAL)
								 .AddShaderStage(vert)
								 .AddShaderStage(frag)
								 .Build(*m_GBufferGenPipelineLayout, true);
		m_GBufferGenPipeline = std::make_unique<vkc::Pipeline>(std::move(pipeline));
	}
	// lighting pipeline
	{
		vkc::ShaderStage quad{ m_Context, help::ReadFile("shaders/quad.spv"), VK_SHADER_STAGE_VERTEX_BIT };
		vkc::ShaderStage lighting{ m_Context, help::ReadFile("shaders/lighting.spv"), VK_SHADER_STAGE_FRAGMENT_BIT };
		lighting.AddSpecializationConstant(static_cast<uint32_t>(m_Scene->GetLights().size()));
		VkBool32 const hasDirectionalLights = m_Scene->GetDirectionalLightCount() > 0 ? VK_TRUE : VK_FALSE;
		VkBool32 const hasPointLights       = m_Scene->GetPointLightCount() > 0 ? VK_TRUE : VK_FALSE;
		lighting.AddSpecializationConstant(std::max(m_Scene->GetDirectionalLightCount(), 1u));
		lighting.AddSpecializationConstant(m_Scene->GetPointLightCount());
		lighting.AddSpecializationConstant(hasDirectionalLights & m_Config.EnableDirectionalLights);
		lighting.AddSpecializationConstant(hasPointLights & m_Config.EnablePointLights);
		lighting.AddSpecializationConstant(SHADOW_FAR_PLANE);

		VkFormat colorAttachmentFormats[]{ m_HDRIRenderTarget->GetFormat() };

		vkc::PipelineBuilder builder{ m_Context };
		vkc::Pipeline        pipeline = builder
								 .SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
								 .AddViewport(m_Context.Swapchain.extent)
								 .SetPolygonMode(VK_POLYGON_MODE_FILL)
								 .SetCullMode(VK_CULL_MODE_NONE)
								 .SetFrontFace(VK_FRONT_FACE_COUNTER_CLOCKWISE)
								 .AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
								 .AddDynamicState(VK_DYNAMIC_STATE_SCISSOR)
								 .AddColorBlendAttachment(blendAttachment)
								 .SetRenderingAttachments(colorAttachmentFormats, m_DepthFormat, VK_FORMAT_UNDEFINED)
								 .AddShaderStage(quad)
								 .AddShaderStage(lighting)
								 .Build(*m_LightingPipelineLayout, true);
		m_LightingPipeline = std::make_unique<vkc::Pipeline>(std::move(pipeline));
	}
	// blit pipeline
	{
		vkc::ShaderStage quad{ m_Context, help::ReadFile("shaders/quad.spv"), VK_SHADER_STAGE_VERTEX_BIT };
		vkc::ShaderStage blit{ m_Context, help::ReadFile("shaders/blit.spv"), VK_SHADER_STAGE_FRAGMENT_BIT };

		VkFormat colorAttachmentFormats[]{ m_Context.Swapchain.image_format };

		vkc::PipelineBuilder builder{ m_Context };
		vkc::Pipeline        pipeline = builder
								 .SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
								 .AddViewport(m_Context.Swapchain.extent)
								 .SetPolygonMode(VK_POLYGON_MODE_FILL)
								 .SetCullMode(VK_CULL_MODE_NONE)
								 .SetFrontFace(VK_FRONT_FACE_COUNTER_CLOCKWISE)
								 .AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
								 .AddDynamicState(VK_DYNAMIC_STATE_SCISSOR)
								 .AddColorBlendAttachment(blendAttachment)
								 .SetRenderingAttachments(colorAttachmentFormats, m_DepthFormat, VK_FORMAT_UNDEFINED)
								 .AddShaderStage(quad)
								 .AddShaderStage(blit)
								 .Build(*m_BlitPipelineLayout, true);
		m_BlitPipeline = std::make_unique<vkc::Pipeline>(std::move(pipeline));
	}
}

void App::CreateCmdPool()
{
	m_CommandPool = std::make_unique<vkc::CommandPool>(m_Context
													   , m_Context.Device.get_queue_index(vkb::QueueType::graphics).value()
													   , m_FramesInFlight
													   , VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
}

void App::CreateScene()
{
	m_Scene = std::make_unique<Scene>(m_Context, *m_CommandPool);
	m_Scene->Load("data/glTF/Sponza.gltf");
	m_Scene->AddLight(-glm::normalize(glm::vec3{ 0.577f, -0.577f, -0.577f }), false, { .877f, .877f, .577f }, 100.f);
	// m_Scene->AddLight(-glm::normalize(glm::vec3{ .999f, -.577f, .0f }), false, { .877f, .877f, .3f }, 50.f);
	m_Scene->AddLight({ -2.f, 1.f, .0f }, true, { 1.f, .0f, .0f }, 125.f);
	m_Scene->AddLight({ -6.f, 1.f, .0f }, true, { .0f, 1.f, .0f }, 75.f);
	m_Scene->AddLight({ 6.f, 1.f, .0f }, true, { .666f, .533f, .12f }, 75.f);
	std::cout << (m_Scene->ContainsPBRInfo() ? "scene contains pbr info" : "scene does not contain pbr info") << std::endl;
}

void App::CreateResources()
{
	// mvp ubo
	{
		vkc::BufferBuilder builder{ m_Context };
		builder.MapMemory().SetMemoryUsage(VMA_MEMORY_USAGE_CPU_TO_GPU);

		for (uint32_t index{}; index < m_FramesInFlight; ++index)
			m_MVPUBOs.emplace_back(builder.Build(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(ModelViewProj)));
	}
	// lights ssbo
	{
		vkc::BufferBuilder builder{ m_Context };
		builder.MapMemory().SetMemoryUsage(VMA_MEMORY_USAGE_CPU_TO_GPU);

		size_t const lightCount         = m_Scene->GetLights().size();
		size_t const lightMatricesCount = m_Scene->GetLightMatrices().size();

		if (lightCount > 0)
			for (uint32_t index{}; index < m_FramesInFlight; ++index)
			{
				m_LightSSBOs.emplace_back(builder.Build(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
														, lightCount * sizeof(Light)));
				m_LightSSBOs[index].UpdateData(m_Scene->GetLights());
				if (!m_Scene->GetLightMatrices().empty())
				{
					m_LightMatricesSSBOs.emplace_back(builder.Build(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
																	, lightMatricesCount * sizeof(glm::mat4)));

					m_LightMatricesSSBOs[index].UpdateData(m_Scene->GetLightMatrices());
					help::NameObject(m_Context
									 , reinterpret_cast<uint64_t>(static_cast<VkBuffer>(m_LightMatricesSSBOs[index]))
									 , VK_OBJECT_TYPE_BUFFER
									 , "Light matrices SSBO");
				}
				help::NameObject(m_Context
								 , reinterpret_cast<uint64_t>(static_cast<VkBuffer>(m_LightSSBOs[index]))
								 , VK_OBJECT_TYPE_BUFFER
								 , "Light SSBO");
			}
	}
	CreateDepth();
	CreateGBuffer();
	m_HDRIRenderTarget = std::make_unique<HDRIRenderTarget>(m_Context);
	m_Context.DeletionQueue.Push([this]
	{
		m_DepthImage->Destroy(m_Context);
		m_DepthImageView->Destroy(m_Context);
		m_AlbedoImage->Destroy(m_Context);
		m_AlbedoView->Destroy(m_Context);
		m_MaterialImage->Destroy(m_Context);
		m_MaterialView->Destroy(m_Context);
		m_HDRIRenderTarget->Destroy(m_Context);
	});
}

void App::CreateGBuffer()
{
	vkc::ImageBuilder builder{ m_Context };

	vkc::Image image = builder
					   .SetExtent(m_Context.Swapchain.extent)
					   .SetFormat(VK_FORMAT_R8G8B8A8_SRGB).SetType(VK_IMAGE_TYPE_2D)
					   .SetAspectFlags(VK_IMAGE_ASPECT_COLOR_BIT)
					   .Build(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, false);
	vkc::ImageView view = image.CreateView(m_Context, VK_IMAGE_VIEW_TYPE_2D, 0, 1, 0, 1, false);

	m_AlbedoImage = std::make_unique<vkc::Image>(std::move(image));
	m_AlbedoView  = std::make_unique<vkc::ImageView>(std::move(view));

	image = builder
			.SetExtent(m_Context.Swapchain.extent)
			.SetFormat(VK_FORMAT_R16G16B16A16_UNORM).SetType(VK_IMAGE_TYPE_2D)
			.SetAspectFlags(VK_IMAGE_ASPECT_COLOR_BIT)
			.Build(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, false);
	view = image.CreateView(m_Context, VK_IMAGE_VIEW_TYPE_2D, 0, 1, 0, 1, false);

	m_MaterialImage = std::make_unique<vkc::Image>(std::move(image));
	m_MaterialView  = std::make_unique<vkc::ImageView>(std::move(view));
}

void App::CreateDepth()
{
	vkc::ImageBuilder builder{ m_Context };
	vkc::Image        image = builder
					   .SetExtent(m_Context.Swapchain.extent)
					   .SetFormat(m_DepthFormat)
					   .SetType(VK_IMAGE_TYPE_2D)
					   .SetAspectFlags(VK_IMAGE_ASPECT_DEPTH_BIT | help::HasStencilComponent(m_DepthFormat) *
									   VK_IMAGE_ASPECT_STENCIL_BIT)
					   .Build(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, false);
	m_DepthImage = std::make_unique<vkc::Image>(std::move(image));

	vkc::ImageView imageView = m_DepthImage->CreateView(m_Context, VK_IMAGE_VIEW_TYPE_2D, 0, 1, 0, 1, false);
	m_DepthImageView         = std::make_unique<vkc::ImageView>(std::move(imageView));
}

void App::RecreateSwapchain()
{
	if (auto const result = m_Context.DispatchTable.deviceWaitIdle();
		result != VK_SUCCESS)
		throw std::runtime_error("Failed to wait for device to be idle");

	std::vector<VkImageView> views;
	views.reserve(m_SwapchainImageViews.size());
	for (uint32_t index{}; index < m_SwapchainImageViews.size(); ++index)
		views.emplace_back(m_SwapchainImageViews[index]);
	m_Context.Swapchain.destroy_image_views(views);

	m_DepthImage->Destroy(m_Context);
	m_DepthImageView->Destroy(m_Context);
	m_AlbedoImage->Destroy(m_Context);
	m_AlbedoView->Destroy(m_Context);
	m_MaterialImage->Destroy(m_Context);
	m_MaterialView->Destroy(m_Context);
	m_HDRIRenderTarget->Destroy(m_Context);

	CreateSwapchain();
	CreateDepth();
	CreateGBuffer();
	m_HDRIRenderTarget = std::make_unique<HDRIRenderTarget>(m_Context);
	m_Camera->SetNewAspectRatio(static_cast<float>(m_Context.Swapchain.extent.width)
								/ m_Context.Swapchain.extent.height); // NOLINT(*-narrowing-conversions)

	UpdateGbufferDescriptor();
}

void App::RecordCommandBuffer(vkc::CommandBuffer& commandBuffer, size_t imageIndex)
{
	using namespace std::placeholders;
	m_QueryPool->RecordWholePipe(commandBuffer
								 , "Depth prepass"
								 , 0
								 , [this, &commandBuffer, imageIndex]
								 {
									 DoDepthPrepass(commandBuffer, imageIndex);
								 });
	m_QueryPool->RecordWholePipe(commandBuffer
								 , "GBuffer generation"
								 , 1
								 , [this, &commandBuffer, imageIndex]
								 {
									 DoGBufferPass(commandBuffer, imageIndex);
								 });
	m_QueryPool->RecordWholePipe(commandBuffer
								 , "Lighting pass"
								 , 2
								 , [this, &commandBuffer, imageIndex]
								 {
									 DoLightingPass(commandBuffer, imageIndex);
								 });
	m_QueryPool->RecordWholePipe(commandBuffer
								 , "Blit pass"
								 , 3
								 , [this, &commandBuffer, imageIndex]
								 {
									 DoBlitPass(commandBuffer, imageIndex);
								 });
}

void App::Submit(vkc::CommandBuffer& commandBuffer) const
{
	VkSemaphoreSubmitInfo waitSemaphoreSubmitInfo{};
	waitSemaphoreSubmitInfo.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	waitSemaphoreSubmitInfo.semaphore = m_ImageAvailableSemaphores[m_CurrentFrame];
	waitSemaphoreSubmitInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

	VkSemaphoreSubmitInfo signalSemaphoreSubmitInfo{};
	signalSemaphoreSubmitInfo.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	signalSemaphoreSubmitInfo.semaphore = m_RenderFinishedSemaphores[m_CurrentFrame];

	VkSemaphoreSubmitInfo waitSemaphoreInfos[]{ waitSemaphoreSubmitInfo };
	VkSemaphoreSubmitInfo signalSemaphoreInfos[]{ signalSemaphoreSubmitInfo };

	commandBuffer.Submit(m_Context, m_Context.GraphicsQueue, waitSemaphoreInfos, signalSemaphoreInfos, m_InFlightFences[m_CurrentFrame]);
}

void App::Present(uint32_t imageIndex)
{
	VkSwapchainKHR const swapchains[]{ m_Context.Swapchain };

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores    = &m_RenderFinishedSemaphores[m_CurrentFrame];
	presentInfo.swapchainCount     = static_cast<uint32_t>(std::size(swapchains));
	presentInfo.pSwapchains        = swapchains;
	presentInfo.pImageIndices      = &imageIndex;

	if (auto const result = m_Context.DispatchTable.queuePresentKHR(m_Context.PresentQueue, &presentInfo);
		result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
		RecreateSwapchain();
}

void App::End()
{
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	m_Context.DeletionQueue.Flush();
}

void App::DoBlitPass(vkc::CommandBuffer& commandBuffer, size_t imageIndex)
{
	VkDebugUtilsLabelEXT debugLabel{};
	debugLabel.sType      = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
	debugLabel.pLabelName = "blit pass";
	static float constexpr color[4]{ .23f, 1.f, .65f, 1.f };
	for (size_t index{}; index < std::size(debugLabel.color); ++index)
		debugLabel.color[index] = color[index];
	m_Context.DispatchTable.cmdBeginDebugUtilsLabelEXT(commandBuffer, &debugLabel);

	vkc::Image& swapchainImage = m_SwapchainImages[imageIndex];

	auto hdrimages = m_HDRIRenderTarget->GetImages();
	// swapchain image to attachment optimal
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
		swapchainImage.MakeTransition(m_Context, commandBuffer, transition);
	}
	// gbuffer images to read only optimal
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
		for (auto& image: hdrimages)
			image.MakeTransition(m_Context, commandBuffer, transition);
	}

	VkRenderingAttachmentInfo renderingAttachmentInfo{};
	renderingAttachmentInfo.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	renderingAttachmentInfo.clearValue  = { { .03f, .03f, .03f, 1.f } };
	renderingAttachmentInfo.imageLayout = m_SwapchainImages[imageIndex].GetLayout();
	renderingAttachmentInfo.imageView   = m_SwapchainImageViews[imageIndex];
	renderingAttachmentInfo.loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR;
	renderingAttachmentInfo.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;

	VkRenderingInfo renderingInfo{};
	renderingInfo.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO;
	renderingInfo.colorAttachmentCount = 1;
	renderingInfo.pColorAttachments    = &renderingAttachmentInfo;
	renderingInfo.layerCount           = 1;
	renderingInfo.renderArea           = VkRect2D{ {}, m_Context.Swapchain.extent };

	m_Context.DispatchTable.cmdBeginRendering(commandBuffer, &renderingInfo);
	// render
	{
		VkViewport viewport{};
		viewport.width    = static_cast<float>(m_Context.Swapchain.extent.width);
		viewport.height   = static_cast<float>(m_Context.Swapchain.extent.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		m_Context.DispatchTable.cmdSetViewport(commandBuffer, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = m_Context.Swapchain.extent;

		m_Context.DispatchTable.cmdSetScissor(commandBuffer, 0, 1, &scissor);

		VkDescriptorSet const sets[]{
			m_GlobalDescriptorSets[m_CurrentFrame]
			, m_GbufferDescriptorSets[m_CurrentFrame]
		};

		m_Context.DispatchTable.cmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_BlitPipeline);
		m_Context.DispatchTable.cmdBindDescriptorSets(commandBuffer
													  , VK_PIPELINE_BIND_POINT_GRAPHICS
													  , *m_BlitPipelineLayout
													  , 0
													  , static_cast<uint32_t>(std::size(sets))
													  , sets
													  , 0
													  , nullptr);
		uint32_t index = m_HDRIRenderTarget->GetCurrentImageIndex();
		m_Context.DispatchTable.cmdPushConstants(commandBuffer
												 , *m_BlitPipelineLayout
												 , VK_SHADER_STAGE_FRAGMENT_BIT
												 , 0
												 , sizeof(uint32_t)
												 , &index);

		m_Context.DispatchTable.cmdDraw(commandBuffer, 3, 1, 0, 0);
	}

	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
	DrawImGui();
	ImGui::Render();
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);

	m_Context.DispatchTable.cmdEndRendering(commandBuffer);

	// swapchain image to present
	{
		vkc::Image::Transition transition{};
		//
		{
			transition.SrcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
			transition.DstAccessMask = VK_ACCESS_2_NONE;
			transition.SrcStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
			transition.DstStageMask  = VK_PIPELINE_STAGE_2_NONE;
			transition.NewLayout     = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		}
		swapchainImage.MakeTransition(m_Context, commandBuffer, transition);
	}
	m_Context.DispatchTable.cmdEndDebugUtilsLabelEXT(commandBuffer);
}

void App::DoLightingPass(vkc::CommandBuffer& commandBuffer, size_t) const
{
	VkDebugUtilsLabelEXT debugLabel{};
	debugLabel.sType      = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
	debugLabel.pLabelName = "lighting pass";
	static float constexpr color[4]{ .23f, 1.f, .65f, 1.f };
	for (size_t index{}; index < std::size(debugLabel.color); ++index)
		debugLabel.color[index] = color[index];
	m_Context.DispatchTable.cmdBeginDebugUtilsLabelEXT(commandBuffer, &debugLabel);
	auto [renderImage, renderImageView] = m_HDRIRenderTarget->AcquireNextTarget();
	// render image to attachment optimal
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
		renderImage->MakeTransition(m_Context, commandBuffer, transition);
	}
	// gbuffer images to read only optimal
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
		m_AlbedoImage->MakeTransition(m_Context, commandBuffer, transition);
		m_MaterialImage->MakeTransition(m_Context, commandBuffer, transition);
		m_DepthImage->MakeTransition(m_Context, commandBuffer, transition);
	}

	VkRenderingAttachmentInfo renderingAttachmentInfo{};
	renderingAttachmentInfo.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	renderingAttachmentInfo.clearValue  = { { .03f, .03f, .03f, 1.f } };
	renderingAttachmentInfo.imageLayout = renderImage->GetLayout();
	renderingAttachmentInfo.imageView   = *renderImageView;
	renderingAttachmentInfo.loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR;
	renderingAttachmentInfo.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;

	VkRenderingInfo renderingInfo{};
	renderingInfo.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO;
	renderingInfo.colorAttachmentCount = 1;
	renderingInfo.pColorAttachments    = &renderingAttachmentInfo;
	renderingInfo.layerCount           = 1;
	renderingInfo.renderArea           = VkRect2D{ {}, m_Context.Swapchain.extent };

	m_Context.DispatchTable.cmdBeginRendering(commandBuffer, &renderingInfo);
	// render
	{
		VkViewport viewport{};
		viewport.width    = static_cast<float>(m_Context.Swapchain.extent.width);
		viewport.height   = static_cast<float>(m_Context.Swapchain.extent.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		m_Context.DispatchTable.cmdSetViewport(commandBuffer, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = m_Context.Swapchain.extent;

		m_Context.DispatchTable.cmdSetScissor(commandBuffer, 0, 1, &scissor);

		VkDescriptorSet const sets[]{
			m_GlobalDescriptorSets[m_CurrentFrame]
			, m_FrameDescriptorSets[m_CurrentFrame]
			, m_GbufferDescriptorSets[m_CurrentFrame]
		};

		m_Context.DispatchTable.cmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_LightingPipeline);
		m_Context.DispatchTable.cmdBindDescriptorSets(commandBuffer
													  , VK_PIPELINE_BIND_POINT_GRAPHICS
													  , *m_LightingPipelineLayout
													  , 0
													  , static_cast<uint32_t>(std::size(sets))
													  , sets
													  , 0
													  , nullptr);

		m_Context.DispatchTable.cmdDraw(commandBuffer, 3, 1, 0, 0);
	}
	m_Context.DispatchTable.cmdEndRendering(commandBuffer);
	m_Context.DispatchTable.cmdEndDebugUtilsLabelEXT(commandBuffer);
}

void App::DoGBufferPass(vkc::CommandBuffer& commandBuffer, size_t) const
{
	VkDebugUtilsLabelEXT debugLabel{};
	debugLabel.sType      = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
	debugLabel.pLabelName = "gbuffer generation";
	static float constexpr color[4]{ .77f, 1.f, .11f, 1.f };
	for (size_t index{}; index < std::size(debugLabel.color); ++index)
		debugLabel.color[index] = color[index];
	m_Context.DispatchTable.cmdBeginDebugUtilsLabelEXT(commandBuffer, &debugLabel);
	// depth image to attachment optimal
	{
		vkc::Image::Transition transition{};
		//
		{
			transition.SrcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			transition.DstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			transition.SrcStageMask  = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
			transition.DstStageMask  = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
			transition.NewLayout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}
		m_DepthImage->MakeTransition(m_Context, commandBuffer, transition);
	}
	// gbuffer images to attachment optimal
	{
		vkc::Image::Transition transition{};
		//
		{
			transition.SrcAccessMask = VK_ACCESS_2_NONE;
			transition.DstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
			transition.SrcStageMask  = VK_PIPELINE_STAGE_2_NONE;
			transition.DstStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
			transition.NewLayout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}
		m_AlbedoImage->MakeTransition(m_Context, commandBuffer, transition);
		m_MaterialImage->MakeTransition(m_Context, commandBuffer, transition);
	}

	VkRenderingAttachmentInfo const renderingAttachmentInfo[]
	{
		{
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO
			, .pNext = nullptr
			, .imageView = *m_AlbedoView
			, .imageLayout = m_AlbedoImage->GetLayout()
			, .resolveMode = VK_RESOLVE_MODE_NONE
			, .resolveImageView = VK_NULL_HANDLE
			, .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED
			, .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR
			, .storeOp = VK_ATTACHMENT_STORE_OP_STORE
			, .clearValue = { { .0f, .0f, .0f, 1.f } }
		}
		, {
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO
			, .pNext = nullptr
			, .imageView = *m_MaterialView
			, .imageLayout = m_MaterialImage->GetLayout()
			, .resolveMode = VK_RESOLVE_MODE_NONE
			, .resolveImageView = VK_NULL_HANDLE
			, .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED
			, .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR
			, .storeOp = VK_ATTACHMENT_STORE_OP_STORE
			, .clearValue = { { .0f, .0f, .0f, 1.f } }
		}
	};

	VkRenderingAttachmentInfo depthAttachmentInfo{};
	depthAttachmentInfo.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	depthAttachmentInfo.clearValue  = { .depthStencil{ 1.f, 0 } };
	depthAttachmentInfo.imageLayout = m_DepthImage->GetLayout();
	depthAttachmentInfo.imageView   = *m_DepthImageView;
	depthAttachmentInfo.loadOp      = VK_ATTACHMENT_LOAD_OP_LOAD;
	depthAttachmentInfo.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;

	VkRenderingInfo renderingInfo{};
	renderingInfo.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO;
	renderingInfo.colorAttachmentCount = static_cast<uint32_t>(std::size(renderingAttachmentInfo));
	renderingInfo.pColorAttachments    = renderingAttachmentInfo;
	renderingInfo.pDepthAttachment     = &depthAttachmentInfo;
	renderingInfo.layerCount           = 1;
	renderingInfo.renderArea           = VkRect2D{ {}, m_AlbedoImage->GetExtent() };

	m_Context.DispatchTable.cmdBeginRendering(commandBuffer, &renderingInfo);
	// render
	{
		VkViewport viewport{};
		viewport.width    = static_cast<float>(m_AlbedoImage->GetExtent().width);
		viewport.height   = static_cast<float>(m_AlbedoImage->GetExtent().height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		m_Context.DispatchTable.cmdSetViewport(commandBuffer, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = m_AlbedoImage->GetExtent();

		m_Context.DispatchTable.cmdSetScissor(commandBuffer, 0, 1, &scissor);

		VkDescriptorSet const sets[]{ m_GlobalDescriptorSets[m_CurrentFrame], m_FrameDescriptorSets[m_CurrentFrame] };

		m_Context.DispatchTable.cmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_GBufferGenPipeline);
		m_Context.DispatchTable.cmdBindDescriptorSets(commandBuffer
													  , VK_PIPELINE_BIND_POINT_GRAPHICS
													  , *m_GBufferGenPipelineLayout
													  , 0
													  , static_cast<uint32_t>(std::size(sets))
													  , sets
													  , 0
													  , nullptr);

		VkDeviceSize constexpr offsets[] = { {} };

		for (auto const& meshes = m_Scene->GetMeshes();
			 Mesh const& mesh: meshes)
		{
			m_Context.DispatchTable.cmdBindVertexBuffers(commandBuffer
														 , 0
														 , 1
														 , mesh.GetVertexBuffer()
														 , offsets);

			m_Context.DispatchTable.cmdBindIndexBuffer(commandBuffer, mesh.GetIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

			m_Context.DispatchTable.cmdPushConstants(commandBuffer
													 , *m_GBufferGenPipelineLayout
													 , VK_SHADER_STAGE_FRAGMENT_BIT
													 , 0
													 , sizeof(TextureIndices)
													 , &mesh.GetTextureIndices());

			m_Context.DispatchTable.cmdDrawIndexed(commandBuffer
												   , static_cast<uint32_t>(mesh.GetIndexBuffer().GetSize() / sizeof(uint32_t))
												   , 1
												   , 0
												   , 0
												   , 0);
		}
	}
	m_Context.DispatchTable.cmdEndRendering(commandBuffer);
	m_Context.DispatchTable.cmdEndDebugUtilsLabelEXT(commandBuffer);
}

void App::DoDepthPrepass(vkc::CommandBuffer const& commandBuffer, size_t) const
{
	std::string const    label{ "Depth prepass" };
	VkDebugUtilsLabelEXT debugLabel{};
	debugLabel.sType      = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
	debugLabel.pLabelName = label.c_str();
	static float constexpr color[4]{ .77f, .77f, .77f, 1.f };
	for (size_t index{}; index < std::size(debugLabel.color); ++index)
		debugLabel.color[index] = color[index];
	m_Context.DispatchTable.cmdBeginDebugUtilsLabelEXT(commandBuffer, &debugLabel);
	// depth image to attachment optimal
	{
		vkc::Image::Transition transition{};
		//
		{
			transition.SrcAccessMask = VK_ACCESS_2_NONE;
			transition.DstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			transition.SrcStageMask  = VK_PIPELINE_STAGE_2_NONE;
			transition.DstStageMask  = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
			transition.NewLayout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}
		m_DepthImage->MakeTransition(m_Context, commandBuffer, transition);
	}

	VkRenderingAttachmentInfo depthAttachmentInfo{};
	depthAttachmentInfo.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	depthAttachmentInfo.clearValue  = { .depthStencil{ 1.f, 0 } };
	depthAttachmentInfo.imageLayout = m_DepthImage->GetLayout();
	depthAttachmentInfo.imageView   = *m_DepthImageView;
	depthAttachmentInfo.loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachmentInfo.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;

	VkRenderingInfo renderingInfo{};
	renderingInfo.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO;
	renderingInfo.colorAttachmentCount = 0;
	renderingInfo.layerCount           = 1;
	renderingInfo.pDepthAttachment     = &depthAttachmentInfo;
	renderingInfo.renderArea           = VkRect2D{ {}, m_Context.Swapchain.extent };

	m_Context.DispatchTable.cmdBeginRendering(commandBuffer, &renderingInfo);
	// render
	{
		VkViewport viewport{};
		viewport.width    = static_cast<float>(m_Context.Swapchain.extent.width);
		viewport.height   = static_cast<float>(m_Context.Swapchain.extent.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		m_Context.DispatchTable.cmdSetViewport(commandBuffer, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = m_Context.Swapchain.extent;

		m_Context.DispatchTable.cmdSetScissor(commandBuffer, 0, 1, &scissor);

		VkDescriptorSet const sets[]{ m_GlobalDescriptorSets[m_CurrentFrame], m_FrameDescriptorSets[m_CurrentFrame] };

		m_Context.DispatchTable.cmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_DepthPrepPipeline);
		m_Context.DispatchTable.cmdBindDescriptorSets(commandBuffer
													  , VK_PIPELINE_BIND_POINT_GRAPHICS
													  , *m_DepthPrepPipelineLayout
													  , 0
													  , static_cast<uint32_t>(std::size(sets))
													  , sets
													  , 0
													  , nullptr);

		VkDeviceSize constexpr offsets[] = { {} };

		for (auto const& meshes = m_Scene->GetMeshes();
			 Mesh const& mesh: meshes)
		{
			m_Context.DispatchTable.cmdBindVertexBuffers(commandBuffer
														 , 0
														 , 1
														 , mesh.GetVertexBuffer()
														 , offsets);

			m_Context.DispatchTable.cmdBindIndexBuffer(commandBuffer, mesh.GetIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

			m_Context.DispatchTable.cmdPushConstants(commandBuffer
													 , *m_DepthPrepPipelineLayout
													 , VK_SHADER_STAGE_FRAGMENT_BIT
													 , 0
													 , sizeof(TextureIndices::Diffuse)
													 , &mesh.GetTextureIndices().Diffuse);

			m_Context.DispatchTable.cmdDrawIndexed(commandBuffer
												   , static_cast<uint32_t>(mesh.GetIndexBuffer().GetSize() / sizeof(uint32_t))
												   , 1
												   , 0
												   , 0
												   , 0);
		}
	}
	m_Context.DispatchTable.cmdEndRendering(commandBuffer);
	m_Context.DispatchTable.cmdEndDebugUtilsLabelEXT(commandBuffer);
}
