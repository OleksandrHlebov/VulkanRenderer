#ifndef APP_H
#define APP_H
#include <memory>

#include "buffer.h"
#include "context.h"
#include "camera.h"
#include "descriptor_set.h"
#include "VkBootstrap.h"

namespace vkc
{
	class CommandPool;
	class CommandBuffer;
	class DescriptorPool;
	class DescriptorSetLayout;
	class ImageView;
	class Image;
	class Pipeline;
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

private:
	void CreateWindow(int width, int height);
	void CreateInstance();
	void CreateSurface();
	void CreateDevice();
	void CreateSwapchain();
	void CreateSyncObjects();
	void CreateDescriptorPool();
	void CreateDescriptorSets();
	void CreateVertexBuffer();
	void CreateGraphicsPipeline();
	void CreateCmdPool();
	void CreateDescriptorSetLayouts();
	void CreateResources();
	void CreateDepth();
	void RecreateSwapchain();
	void RecordCommandBuffer(vkc::CommandBuffer& commandBuffer, size_t imageIndex);
	void Submit(vkc::CommandBuffer& commandBuffer) const;
	void Present(uint32_t imageIndex);
	void End();

	uptr<Camera> m_Camera;
	vkc::Context m_Context{};

	uptr<vkc::DescriptorSetLayout> m_FrameDescSetLayout{};
	uptr<vkc::DescriptorPool>      m_DescPool{};

	uptr<vkc::PipelineLayout> m_PipelineLayout;
	uptr<vkc::Pipeline>       m_Pipeline{};

	VkFormat             m_DepthFormat{};
	uptr<vkc::Image>     m_DepthImage{};
	uptr<vkc::ImageView> m_DepthImageView{};

	std::vector<vkc::Image>     m_SwapchainImages;
	std::vector<vkc::ImageView> m_SwapchainImageViews;

	uptr<vkc::CommandPool> m_CommandPool{};

	uptr<vkc::Buffer>        m_VertexBuffer{};
	std::vector<vkc::Buffer> m_MVPUBOs{};

	std::vector<vkc::DescriptorSet> m_FrameDescriptorSets{};

	std::vector<VkSemaphore> m_ImageAvailableSemaphores{};
	std::vector<VkSemaphore> m_RenderFinishedSemaphores{};
	std::vector<VkFence>     m_InFlightFences{};

	uint32_t m_FramesInFlight{};
	uint32_t m_CurrentFrame{};
};

#endif //APP_H
