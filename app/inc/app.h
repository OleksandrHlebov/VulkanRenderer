#ifndef APP_H
#define APP_H
#include <memory>

#include "buffer.h"
#include "context.h"
#include "camera.h"
#include "image.h"
#include "VkBootstrap.h"

class App final
{
public:
	template<typename T>
	using uptr = std::unique_ptr<T>;

	App(int width, int height);
	~App() = default;

	App(App&&)                 = delete;
	App(App const&)            = delete;
	App& operator=(App const&) = delete;
	App& operator=(App&&)      = delete;

	void Run();

private:
	// TODO: specialisation
	[[nodiscard]] VkShaderModule CreateShaderModule(std::vector<char> code) const;

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
	void CreateCommandBuffers();
	void RecordCommandBuffer(VkCommandBuffer commandBuffer, size_t imageIndex) const;
	void Submit() const;
	void Present(uint32_t imageIndex) const;
	void End();

	std::unique_ptr<Camera> m_Camera;
	Context                 m_Context{};

	VkDescriptorSetLayout m_FrameDescSetLayout{};
	VkDescriptorPool      m_DescPool{};

	VkPipelineLayout m_PipelineLayout{};
	VkPipeline       m_Pipeline{};

	VkFormat        m_DepthFormat{};
	uptr<Image>     m_DepthImage{};
	uptr<ImageView> m_DepthImageView{};

	std::vector<Image>     m_SwapchainImages;
	std::vector<ImageView> m_SwapchainImageViews;

	std::vector<VkCommandBuffer> m_CommandBuffers{};

	std::vector<Buffer> m_MVPUBOs{};

	std::vector<VkDescriptorSet> m_FrameDescriptorSets{};

	std::vector<VkSemaphore> m_ImageAvailableSemaphores{};
	std::vector<VkSemaphore> m_RenderFinishedSemaphores{};
	std::vector<VkFence>     m_InFlightFences{};

	uint32_t m_FramesInFlight{};
	uint32_t m_CurrentFrame{};
};

#endif //APP_H
