#ifndef APP_H
#define APP_H
#include <memory>

#include "buffer.h"
#include "context.h"
#include "camera.h"
#include "pipeline.h"
#include "descriptor_set.h"
#include "descriptor_pool.h"
#include "descriptor_set_layout.h"
#include "VkBootstrap.h"

class Scene;
class CommandPool;
class CommandBuffer;
class ImageView;
class Image;
class PipelineLayout;

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

private:
	void CreateWindow(int width, int height);
	void CreateInstance();
	void CreateSurface();
	void CreateDevice();
	void CreateSwapchain();
	void CreateSyncObjects();
	void CreateDescriptorPool();
	void CreateDescriptorSets();
	void CreateGraphicsPipeline();
	void CreateCmdPool();
	void CreateDescriptorSetLayouts();
	void CreateResources();
	void CreateDepth();
	void RecreateSwapchain();
	void RecordCommandBuffer(CommandBuffer& commandBuffer, size_t imageIndex);
	void Submit(CommandBuffer& commandBuffer) const;
	void Present(uint32_t imageIndex);
	void End();

	uptr<Camera> m_Camera;
	Context      m_Context{};

	uptr<Scene> m_Scene;

	uptr<DescriptorSetLayout> m_FrameDescSetLayout{};
	uptr<DescriptorSetLayout> m_GlobalDescSetLayout{};
	uptr<DescriptorPool>      m_DescPool{};

	uptr<PipelineLayout> m_PipelineLayout;
	uptr<Pipeline>       m_Pipeline{};

	VkFormat        m_DepthFormat{};
	uptr<Image>     m_DepthImage{};
	uptr<ImageView> m_DepthImageView{};

	VkSampler m_TextureSampler{};

	std::vector<Image>     m_SwapchainImages;
	std::vector<ImageView> m_SwapchainImageViews;

	uptr<CommandPool> m_CommandPool{};

	std::vector<Buffer> m_MVPUBOs{};

	std::vector<DescriptorSet> m_FrameDescriptorSets{};
	std::vector<DescriptorSet> m_GlobalDescriptorSets{};

	std::vector<VkSemaphore> m_ImageAvailableSemaphores{};
	std::vector<VkSemaphore> m_RenderFinishedSemaphores{};
	std::vector<VkFence>     m_InFlightFences{};

	uint32_t m_FramesInFlight{};
	uint32_t m_CurrentFrame{};
};

#endif //APP_H
