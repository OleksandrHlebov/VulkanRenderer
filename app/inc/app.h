#ifndef APP_H
#define APP_H
#include "Context.h"
#include "VkBootstrap.h"

class App final
{
public:
	App(int width, int height);
	~App() = default;

	App(App&&)                 = delete;
	App(App const&)            = delete;
	App& operator=(App const&) = delete;
	App& operator=(App&&)      = delete;

	void Run();
	void Present(uint32_t imageIndex) const;

private:
	// TODO: specialisation
	[[nodiscard]] VkShaderModule CreateShaderModule(std::vector<char> code) const;

	void CreateInstance();
	void CreateSurface();
	void CreateDevice();
	void CreateCmdPool();
	void CreateCommandBuffers();
	void CreateWindow(int width, int height);
	void End() const;
	void CreateSwapchain();
	void CreateSyncObjects();
	void CreateGraphicsPipeline();
	void RecordCommandBuffer(VkCommandBuffer const& commandBuffer, size_t imageIndex) const;
	void Submit() const;

	Context m_Context{};

	VkPipelineLayout m_PipelineLayout{};
	VkPipeline       m_Pipeline{};
	VkCommandPool    m_CommandPool{};

	std::vector<VkCommandBuffer> m_CommandBuffers{};

	std::vector<VkSemaphore> m_ImageAvailableSemaphores{};
	std::vector<VkSemaphore> m_RenderFinishedSemaphores{};
	std::vector<VkFence>     m_InFlightFences{};

	size_t m_FramesInFlight{};
	size_t m_CurrentFrame{};
};

#endif //APP_H
