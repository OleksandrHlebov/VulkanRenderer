#ifndef APP_H
#define APP_H
#include "context.h"
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

private:
	// TODO: specialisation
	[[nodiscard]] VkShaderModule CreateShaderModule(std::vector<char> code) const;

	void CreateWindow(int width, int height);
	void CreateInstance();
	void CreateSurface();
	void CreateDevice();
	void CreateSwapchain();
	void CreateSyncObjects();
	void CreateGraphicsPipeline();
	void CreateCmdPool();
	void CreateCommandBuffers();
	void RecordCommandBuffer(VkCommandBuffer const& commandBuffer, size_t imageIndex) const;
	void Submit() const;
	void Present(uint32_t imageIndex) const;
	void End();

	Context m_Context{};

	VkPipelineLayout m_PipelineLayout{};
	VkPipeline       m_Pipeline{};
	VkCommandPool    m_CommandPool{};

	std::vector<VkCommandBuffer> m_CommandBuffers{};

	std::vector<VkSemaphore> m_ImageAvailableSemaphores{};
	std::vector<VkSemaphore> m_RenderFinishedSemaphores{};
	std::vector<VkFence>     m_InFlightFences{};

	uint32_t m_FramesInFlight{};
	uint32_t m_CurrentFrame{};
};

#endif //APP_H
