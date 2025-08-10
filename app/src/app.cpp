#include "app.h"

#include "command_pool.h"
#include "datatypes.h"
#include "helper.h"
#include "image.h"
#include "shader_stage.h"

#include <span>

#include "scene.h"

App::App(int width, int height)
{
	m_Camera = std::make_unique<Camera>(glm::vec3(.0f, .0f, .0f)
										, 45.f
										, static_cast<float>(width) / height // NOLINT(*-narrowing-conversions)
										, .01f
										, 1000.f);
	CreateWindow(width, height);
	CreateInstance();
	CreateSurface();
	CreateDevice();
	CreateSwapchain();
	m_Context.DeletionQueue.Push([this]
	{
		m_DepthImage->Destroy(m_Context);
		m_DepthImageView->Destroy(m_Context);

		std::vector<VkImageView> views;
		views.reserve(m_SwapchainImageViews.size());
		for (uint32_t index{}; index < m_SwapchainImageViews.size(); ++index)
			views.emplace_back(m_SwapchainImageViews[index]);
		m_Context.Swapchain.destroy_image_views(views);
		vkb::destroy_swapchain(m_Context.Swapchain);
	});
	CreateCmdPool();
	// TODO: Load scene
	m_Scene = std::make_unique<Scene>(m_Context, *m_CommandPool);
	m_Scene->LoadScene("data/glTF/Fox.gltf");
	// CreateVertexBuffer();
	CreateDescriptorSetLayouts();
	// TODO: Create resources (depth/textures)
	CreateResources();
	CreateGraphicsPipeline();
	CreateSyncObjects();
	CreateDescriptorPool();
	CreateDescriptorSets();
}

App::~App() = default;

void App::Run()
{
	// main loop
	while (!glfwWindowShouldClose(m_Context.Window))
	{
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

		RecordCommandBuffer(commandBuffer, imageIndex);
		Submit(commandBuffer);

		Present(imageIndex);

		++m_CurrentFrame;
		m_CurrentFrame %= m_FramesInFlight;
	}

	if (m_Context.DispatchTable.deviceWaitIdle() != VK_SUCCESS)
		throw std::runtime_error("Failed to wait for the device");

	End();
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
	features12.sType                                     = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	features12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
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
	m_Context.DeletionQueue.Push([this]
	{
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
										  .AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
										  .Build();

		m_FrameDescSetLayout = std::make_unique<vkc::DescriptorSetLayout>(std::move(layout));
	}
	//
	{
		VkSamplerCreateInfo samplerCreateInfo{};
		samplerCreateInfo.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerCreateInfo.minFilter    = VK_FILTER_LINEAR;
		samplerCreateInfo.magFilter    = VK_FILTER_LINEAR;
		samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

		m_Context.DispatchTable.createSampler(&samplerCreateInfo, nullptr, &m_TextureSampler);

		m_Context.DeletionQueue.Push([this]
		{
			m_Context.DispatchTable.destroySampler(m_TextureSampler, nullptr);
		});

		vkc::DescriptorSetLayoutBuilder builder{ m_Context };
		vkc::DescriptorSetLayout        layout = builder
										  .AddBinding(0, VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
										  .AddBinding(1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT)
										  .Build();
		m_GlobalDescSetLayout = std::make_unique<vkc::DescriptorSetLayout>(std::move(layout));
	}
}

void App::CreateDescriptorPool()
{
	vkc::DescriptorPoolBuilder builder{ m_Context };
	vkc::DescriptorPool        pool = builder
							   .AddPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, m_FramesInFlight) // mvp
							   .AddPoolSize(VK_DESCRIPTOR_TYPE_SAMPLER, m_FramesInFlight)        // sampler
							   .AddPoolSize(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, m_FramesInFlight)
							   .Build(2 * m_FramesInFlight);

	m_DescPool = std::make_unique<vkc::DescriptorPool>(std::move(pool));
}

void App::CreateDescriptorSets()
{
	//
	{
		std::vector<VkDescriptorSetLayout> layouts(m_FramesInFlight, *m_FrameDescSetLayout);

		vkc::DescriptorSetBuilder const builder{ m_Context };
		m_FrameDescriptorSets = builder.Build(*m_DescPool, layouts);

		for (uint32_t index{}; index < m_FramesInFlight; ++index)
		{
			VkDescriptorBufferInfo bufferInfo{};
			bufferInfo.buffer = m_MVPUBOs[index];
			bufferInfo.range  = VK_WHOLE_SIZE;
			bufferInfo.offset = 0;

			VkDescriptorBufferInfo infos[]{ bufferInfo };

			m_FrameDescriptorSets[index]
				.AddWriteDescriptor(infos, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, 0)
				.Update(m_Context);
		}
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

		vkc::DescriptorSetBuilder const builder{ m_Context };
		m_GlobalDescriptorSets = builder.Build(*m_DescPool, layouts);

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
	// default layout
	{
		vkc::PipelineLayoutBuilder builder{ m_Context };
		vkc::PipelineLayout        layout = builder
									 .AddDescriptorSetLayout(*m_GlobalDescSetLayout)
									 .AddDescriptorSetLayout(*m_FrameDescSetLayout)
									 .AddPushConstant(VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(TextureIndices))
									 .Build();
		m_PipelineLayout = std::make_unique<vkc::PipelineLayout>(std::move(layout));
	}

	vkc::ShaderStage const vert{ m_Context, help::ReadFile("shaders/basic_transform.spv"), VK_SHADER_STAGE_VERTEX_BIT };
	vkc::ShaderStage       frag{ m_Context, help::ReadFile("shaders/basic_color.spv"), VK_SHADER_STAGE_FRAGMENT_BIT };
	frag.AddSpecializationConstant(static_cast<uint32_t>(m_Scene->GetTextureImages().size())); // constant must be 4 bytes in size

	VkFormat colorAttachmentFormats[]{ m_Context.Swapchain.image_format };

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
							 .SetRenderingAttachments(colorAttachmentFormats, m_DepthFormat, VK_FORMAT_UNDEFINED)
							 .EnableDepthTest(VK_COMPARE_OP_LESS)
							 .EnableDepthWrite()
							 .AddShaderStage(vert)
							 .AddShaderStage(frag)
							 .Build(*m_PipelineLayout, true);
	m_Pipeline = std::make_unique<vkc::Pipeline>(std::move(pipeline));
}

void App::CreateCmdPool()
{
	m_CommandPool = std::make_unique<vkc::CommandPool>(m_Context
													   , m_Context.Device.get_queue_index(vkb::QueueType::graphics).value()
													   , m_FramesInFlight
													   , VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
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
	CreateDepth();
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
					   .Build(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, false);
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

	CreateSwapchain();
	CreateDepth();
	m_Camera->SetNewAspectRatio(static_cast<float>(m_Context.Swapchain.extent.width)
								/ m_Context.Swapchain.extent.height); // NOLINT(*-narrowing-conversions)
}

void App::RecordCommandBuffer(vkc::CommandBuffer& commandBuffer, size_t imageIndex)
{
	commandBuffer.Begin(m_Context);
	vkc::Image& swapchainImage = m_SwapchainImages[imageIndex];
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

	VkRenderingAttachmentInfo renderingAttachmentInfo{};
	renderingAttachmentInfo.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	renderingAttachmentInfo.clearValue  = { { .03f, .03f, .03f, 1.f } };
	renderingAttachmentInfo.imageLayout = m_SwapchainImages[imageIndex].GetLayout();
	renderingAttachmentInfo.imageView   = m_SwapchainImageViews[imageIndex];
	renderingAttachmentInfo.loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR;
	renderingAttachmentInfo.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;

	VkClearValue clearValue{};
	clearValue.color        = { { 0.0f, 0.0f, 0.0f, 1.0f } };
	clearValue.depthStencil = { 1.0f, 0 };

	VkRenderingAttachmentInfo depthAttachmentInfo{};
	depthAttachmentInfo.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	depthAttachmentInfo.clearValue  = clearValue;
	depthAttachmentInfo.imageLayout = m_DepthImage->GetLayout();
	depthAttachmentInfo.imageView   = *m_DepthImageView;
	depthAttachmentInfo.loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachmentInfo.storeOp     = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	VkRenderingInfo renderingInfo{};
	renderingInfo.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO;
	renderingInfo.colorAttachmentCount = 1;
	renderingInfo.pColorAttachments    = &renderingAttachmentInfo;
	renderingInfo.pDepthAttachment     = &depthAttachmentInfo;
	renderingInfo.layerCount           = 1;
	renderingInfo.renderArea           = VkRect2D{ {}, m_Context.Swapchain.extent };

	m_Context.DispatchTable.cmdBeginRendering(commandBuffer, &renderingInfo);
	// main pass
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

		VkDescriptorSet sets[]{ m_GlobalDescriptorSets[m_CurrentFrame], m_FrameDescriptorSets[m_CurrentFrame] };

		m_Context.DispatchTable.cmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_Pipeline);
		m_Context.DispatchTable.cmdBindDescriptorSets(commandBuffer
													  , VK_PIPELINE_BIND_POINT_GRAPHICS
													  , *m_PipelineLayout
													  , 0
													  , static_cast<uint32_t>(std::size(sets))
													  , sets
													  , 0
													  , nullptr);

		VkDeviceSize offsets[] = { {} };

		//
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
													 , *m_PipelineLayout
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
	commandBuffer.End(m_Context);
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
	m_Context.DeletionQueue.Flush();
}
