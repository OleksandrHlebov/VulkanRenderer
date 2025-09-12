#include "TimingQueryPool.h"
#include <ratio>
#include <limits>
#include <ranges>

TimingQueryPool::TimingQueryPool(vkc::Context const& context, float timeStampPeriod, uint32_t maxQueries)
	: m_TimestampPeriod{ timeStampPeriod }
{
	VkQueryPoolCreateInfo createInfo{};
	createInfo.sType      = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
	createInfo.queryType  = VK_QUERY_TYPE_TIMESTAMP;
	createInfo.queryCount = maxQueries;
	context.DispatchTable.createQueryPool(&createInfo, nullptr, &m_QueryPool);
	m_Timestamps.reserve(maxQueries);
}

void TimingQueryPool::Destroy(vkc::Context const& context) const
{
	context.DispatchTable.destroyQueryPool(m_QueryPool, nullptr);
}

void TimingQueryPool::Reset(vkc::CommandBuffer const& commandBuffer)
{
	vkCmdResetQueryPool(commandBuffer
						, m_QueryPool
						, 0
						, static_cast<uint32_t>(m_Timestamps.capacity()));
	m_Timestamps.clear();
	m_LabeledResults.clear();
}

void TimingQueryPool::WriteTimestamp
(vkc::CommandBuffer const& commandBuffer, VkPipelineStageFlagBits stage, std::string const& label, int priority)
{
	assert(m_Timestamps.size() < m_Timestamps.capacity() && "pool is full");

	vkCmdWriteTimestamp(commandBuffer, stage, m_QueryPool, static_cast<uint32_t>(m_Timestamps.size()));
	if (!m_LabeledResults.contains(priority))
	{
		m_LabeledResults[priority] = InternalTiming{
			label
			, { static_cast<uint32_t>(m_Timestamps.size()), std::numeric_limits<uint32_t>::max() }
		};
	}
	else
	{
		auto& [Label, TimestampIndices] = m_LabeledResults[priority];
		assert(TimestampIndices.second == std::numeric_limits<uint32_t>::max() &&
			   "priority has been already recorded to, reset before making another recording");
		TimestampIndices.second = static_cast<uint32_t>(m_Timestamps.size());
	}
	m_Timestamps.emplace_back();
}

void TimingQueryPool::GetResults(vkc::Context const& context, Timings& outResult)
{
	VkResult const pullResult = context.DispatchTable.getQueryPoolResults(m_QueryPool
																		  , 0
																		  , static_cast<uint32_t>(m_Timestamps.size())
																		  , m_Timestamps.size() * sizeof(uint64_t)
																		  , m_Timestamps.data()
																		  , sizeof(uint64_t)
																		  , VK_QUERY_RESULT_64_BIT);
	if (pullResult == VK_SUCCESS)
	{
		for (auto const& [priority, timing]: m_LabeledResults)
		{
			assert(timing.TimestampIndices.second != std::numeric_limits<uint32_t>::max() && "end of timestamp was not recorded");
			assert(timing.TimestampIndices.second < m_Timestamps.size() && "invalid end index");

			outResult[priority] = timing.CalculateTiming(m_Timestamps, m_TimestampPeriod);
		}
	}
}

Timing TimingQueryPool::InternalTiming::CalculateTiming(std::vector<uint64_t> const& timestamps, float period) const
{
	auto const& [start, end] = TimestampIndices;
	return Timing{ Label, (timestamps[end] - timestamps[start]) * period / static_cast<double>(std::micro::den) };
}
