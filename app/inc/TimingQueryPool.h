#ifndef VULKANRESEARCH_TIMINGQUERYPOOL_H
#define VULKANRESEARCH_TIMINGQUERYPOOL_H

#include "context.h"
#include "command_buffer.h"
#include <map>

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

using Timings = std::map<std::string, double, TimingsCompare>;

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

	void WriteTimestamp(vkc::CommandBuffer const& commandBuffer, VkPipelineStageFlagBits stage, std::string const& label);

	void GetResults(vkc::Context const& context, Timings& outResult);

private:
	VkQueryPool                                                    m_QueryPool{};
	std::vector<uint64_t>                                          m_Timestamps;
	std::unordered_map<std::string, std::pair<uint32_t, uint32_t>> m_LabeledResults;
	float                                                          m_TimestampPeriod;
};

#endif //VULKANRESEARCH_TIMINGQUERYPOOL_H
