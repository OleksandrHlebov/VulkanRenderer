#ifndef APP_H
#define APP_H
#include <memory>

#include "buffer.h"
#include "context.h"
#include "camera.h"
#include "datatypes.h"
#include "pipeline.h"
#include "descriptor_set.h"
#include "descriptor_pool.h"
#include "descriptor_set_layout.h"
#include "pingpong_render_target.h"
#include "VkBootstrap.h"
#include "timing_query_pool.h"

#include <map>

#include "post_processing_effect.h"

class Scene;

namespace vkc
{
	class CommandPool;
	class CommandBuffer;
	class ImageView;
	class Image;
	class PipelineLayout;
}

class App final
{
public:
	template<typename T>
	using uptr = std::unique_ptr<T>;

	App(int width, int height);
	~App();

	App(App&&)                 = delete;
	App(App const&)            = delete;
	App& operator=(App const&) = delete;
	App& operator=(App&&)      = delete;

	void Run();

	static float constexpr SHADOW_FAR_PLANE = 100.0f;

private:
	void InitImGUI() const;
	void DrawImGui();
	void CreateWindow(int width, int height);
	void CreateScene();
	void CreateInstance();
	void CreatePipelineCache();
	void CreateSurface();
	void CreateDevice();
	void CreateSwapchain();
	void CreateSyncObjects();
	void CreateDescriptorPool();
	void UpdateGbufferDescriptor();
	void CreateDescriptorSets();
	void CreateGraphicsPipeline();
	void CreateCmdPool();
	void CreateDescriptorSetLayouts();
	void LoadPostProcessingEffects();
	void GenerateShadowMaps();
	void CreateResources();
	void CreateGBuffer();
	void CreateDepth();
	void RecreateSwapchain();
	void RecordCommandBuffer(vkc::CommandBuffer& commandBuffer, size_t imageIndex);
	void Submit(vkc::CommandBuffer& commandBuffer) const;
	void Present(uint32_t imageIndex);
	void End();

	void DoPostProcessing(vkc::CommandBuffer& commandBuffer, size_t imageIndex, int& priority);
	void DoImGUIPass(vkc::CommandBuffer& commandBuffer, size_t imageIndex);
	void DoBlitPass(vkc::CommandBuffer& commandBuffer, size_t imageIndex);
	void DoLightingPass(vkc::CommandBuffer& commandBuffer, size_t imageIndex) const;
	void DoGBufferPass(vkc::CommandBuffer& commandBuffer, size_t imageIndex) const;
	void DoDepthPrepass(vkc::CommandBuffer const& commandBuffer, size_t imageIndex) const;

	Config                m_Config;
	uptr<TimingQueryPool> m_QueryPool;
	Timings               m_GPUTimings;
	Timings               m_CPUTimings;

	vkb::PhysicalDevice m_PhysicalDevice;

	uptr<Camera> m_Camera;
	vkc::Context m_Context{};

	uptr<Scene> m_Scene;

	uptr<vkc::DescriptorSetLayout> m_FrameDescSetLayout{};
	uptr<vkc::DescriptorSetLayout> m_GlobalDescSetLayout{};
	uptr<vkc::DescriptorSetLayout> m_GbufferDescSetLayout{};
	uptr<vkc::DescriptorPool>      m_DescPool{};

	uptr<vkc::PipelineCache> m_PipelineCache{};

	uptr<vkc::PipelineLayout> m_DepthPrepPipelineLayout;
	uptr<vkc::Pipeline>       m_DepthPrepPipeline{};

	uptr<vkc::PipelineLayout> m_GBufferGenPipelineLayout;
	uptr<vkc::Pipeline>       m_GBufferGenPipeline{};

	uptr<vkc::PipelineLayout> m_LightingPipelineLayout;
	uptr<vkc::Pipeline>       m_LightingPipeline{};

	uptr<vkc::PipelineLayout> m_BlitPipelineLayout;
	uptr<vkc::Pipeline>       m_BlitPipeline{};

	VkFormat             m_DepthFormat{};
	uptr<vkc::Image>     m_DepthImage{};
	uptr<vkc::ImageView> m_DepthImageView{};

	uptr<vkc::Image>     m_AlbedoImage{};
	uptr<vkc::ImageView> m_AlbedoView{};

	uptr<vkc::Image>     m_MaterialImage{};
	uptr<vkc::ImageView> m_MaterialView{};

	uptr<PingPongRenderTarget> m_HDRIRenderTarget{};
	uptr<PingPongRenderTarget> m_SDRRenderTarget{};

	VkSampler m_TextureSampler{};
	VkSampler m_ShadowSampler{};

	std::vector<vkc::Image>     m_SwapchainImages;
	std::vector<vkc::ImageView> m_SwapchainImageViews;

	uptr<vkc::CommandPool> m_CommandPool{};

	std::vector<vkc::Buffer> m_MVPUBOs{};
	std::vector<vkc::Buffer> m_LightSSBOs{};
	std::vector<vkc::Buffer> m_LightMatricesSSBOs{};

	std::vector<vkc::DescriptorSet> m_FrameDescriptorSets{};
	std::vector<vkc::DescriptorSet> m_GlobalDescriptorSets{};
	std::vector<vkc::DescriptorSet> m_GbufferDescriptorSets{};

	std::vector<PostProcessingEffect> m_SDREffects{};

	std::vector<VkSemaphore> m_ImageAvailableSemaphores{};
	std::vector<VkSemaphore> m_RenderFinishedSemaphores{};
	std::vector<VkFence>     m_InFlightFences{};

	uint32_t m_FramesInFlight{};
	uint32_t m_CurrentFrame{};
};

#endif //APP_H
