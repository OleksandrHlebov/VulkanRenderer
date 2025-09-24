#ifndef VULKANRESEARCH_POSTPROCESSINGEFFECT_H
#define VULKANRESEARCH_POSTPROCESSINGEFFECT_H

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "delegate.h"
#include "spirv_reflect.h"

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

	struct ShaderVariable
	{
		std::string               Name;
		SpvReflectTypeFlags const Type;
		size_t                    Size;
		bool                      IsSigned;
		char* const               DataAddress;
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

	void DrawImGUI();

	bool operator<(PostProcessingEffect const& other) const
	{
		return this->m_Name < other.m_Name;
	}

	[[nodiscard]] bool IsEnabled() const
	{
		return m_Enabled;
	}

	[[nodiscard]] std::string const& GetName() const
	{
		return m_Name;
	}

	Event<PostProcessingEffect&> OnToggle{};

private:
	inline static uint32_t m_Counter{};

	uint32_t const m_ID = m_Counter++;

	static void InitializePushConstant
	(std::unordered_map<std::string, std::string>& defaultValues, SpvReflectBlockVariable const& member, ShaderVariable& variable);
	template<typename T>
	using uptr = std::unique_ptr<T>;
	std::string               m_Name;
	uptr<vkc::Pipeline>       m_Pipeline;
	uptr<vkc::PipelineLayout> m_PipelineLayout;

	DescriptorData              m_DescriptorData;
	std::vector<char>           m_PushConstants;
	std::vector<ShaderVariable> m_ShaderVariables;
	bool                        m_Enabled{ true };
};

#endif //VULKANRESEARCH_POSTPROCESSINGEFFECT_H
