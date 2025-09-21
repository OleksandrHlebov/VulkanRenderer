#ifndef VULKANRESEARCH_POSTPROCESSINGEFFECT_H
#define VULKANRESEARCH_POSTPROCESSINGEFFECT_H

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

class PingPongRenderTarget;

namespace vkc
{
	class DescriptorSetLayout;
	class DescriptorSet;
	class ImageView;
	class Image;
	class CommandBuffer;
	class PipelineCache;
	class PipelineLayout;
	class Pipeline;
	struct Context;
}

class PostProcessingEffect final
{
public:
	enum class Type
	{
		SDR, HDR
	};

	struct DescriptorData
	{
		vkc::DescriptorSetLayout const&        GBufferSetLayout;
		std::vector<vkc::DescriptorSet> const& GBufferSet;
		vkc::DescriptorSetLayout const&        GlobalSetLayout;
		std::vector<vkc::DescriptorSet> const& GlobalSet;
	};

	struct RenderData
	{
		vkc::Image&           SwapchainImage;
		vkc::ImageView&       SwapchainImageView;
		PingPongRenderTarget& PingPongTarget;
	};

	PostProcessingEffect() = delete;
	PostProcessingEffect
	(
		vkc::Context&                  context, DescriptorData const& descriptorData
		, vkc::PipelineCache&          pipelineCache
		, Type                         type
		, std::filesystem::path const& shaderPath
	);
	~PostProcessingEffect() = default;

	PostProcessingEffect(PostProcessingEffect&&)                 = default;
	PostProcessingEffect(PostProcessingEffect const&)            = delete;
	PostProcessingEffect& operator=(PostProcessingEffect&&)      = delete;
	PostProcessingEffect& operator=(PostProcessingEffect const&) = delete;

	void Render
	(
		vkc::Context& context, vkc::CommandBuffer& commandBuffer, RenderData& renderData
		, uint32_t    frameIndex, bool             renderToSwapchain
	);

	std::string const& GetName() const
	{
		return m_Name;
	}

private:
	template<typename T>
	using uptr = std::unique_ptr<T>;
	std::string               m_Name;
	uptr<vkc::Pipeline>       m_Pipeline;
	uptr<vkc::PipelineLayout> m_PipelineLayout;

	DescriptorData    m_DescriptorData;
	std::vector<char> m_PushConstants;
};

#endif //VULKANRESEARCH_POSTPROCESSINGEFFECT_H
