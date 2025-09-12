#ifndef VULKANRESEARCH_TIMINGQUERYPOOL_H
#define VULKANRESEARCH_TIMINGQUERYPOOL_H

#include "context.h"
#include "command_buffer.h"
#include <map>

class Timing
{
public:
	Timing() = default;

	Timing(std::string const& label, double duration)
		: m_Label{ label }
		, m_Duration{ duration } {}

	[[nodiscard]] double GetDuration() const
	{
		return m_Duration;
	}

	[[nodiscard]] std::string_view GetLabel() const
	{
		return m_Label;
	}

private:
	std::string m_Label{};
	double      m_Duration{};
};

class TimingsCompare
{
public:
	bool operator()(std::string const& a, std::string const& b) const
	{
		assert(a[0] == '#' &&
			   b[0] == '#' &&
			   "invalid timing entry");
		return std::stoi(a.substr(1)) < std::stoi(b.substr(1));
	}
};

using Timings = std::map<int, Timing>;

class TimingQueryPool
{
	static uint32_t constexpr DEFAULT_MAX_QUERIES = 32;

public:
	TimingQueryPool() = delete;
	TimingQueryPool(vkc::Context const& context, float timeStampPeriod, uint32_t maxQueries = DEFAULT_MAX_QUERIES);
	~TimingQueryPool() = default;

	TimingQueryPool(TimingQueryPool&&)                 = delete;
	TimingQueryPool(TimingQueryPool const&)            = delete;
	TimingQueryPool& operator=(TimingQueryPool&&)      = delete;
	TimingQueryPool& operator=(TimingQueryPool const&) = delete;

	void Destroy(vkc::Context const& context) const;

	void Reset(vkc::CommandBuffer const& commandBuffer);

	void WriteTimestamp(vkc::CommandBuffer const& commandBuffer, VkPipelineStageFlagBits stage, std::string const& label, int priority);

	void GetResults(vkc::Context const& context, Timings& outResult);

	template<typename FunctionType, typename... Args>
	void RecordWholePipe
	(
		vkc::CommandBuffer const& commandBuffer, std::string const& label, int priority
		, FunctionType            function, Args&&...               args
	)
	{
		WriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, label, priority);
		function(std::forward<Args>(args)...);
		WriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, label, priority);
	}

private:
	struct InternalTiming
	{
		std::string                   Label;
		std::pair<uint32_t, uint32_t> TimestampIndices;

		[[nodiscard]] Timing CalculateTiming(std::vector<uint64_t> const& timestamps, float period) const;
	};

	VkQueryPool                   m_QueryPool{};
	std::vector<uint64_t>         m_Timestamps;
	std::map<int, InternalTiming> m_LabeledResults;
	float                         m_TimestampPeriod;
};

#endif //VULKANRESEARCH_TIMINGQUERYPOOL_H
