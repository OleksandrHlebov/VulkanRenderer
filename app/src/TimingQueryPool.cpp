#include "TimingQueryPool.h"
#include <ratio>
#include <limits>

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

void TimingQueryPool::WriteTimestamp(vkc::CommandBuffer const& commandBuffer, VkPipelineStageFlagBits stage, std::string const& label)
{
	assert(m_Timestamps.size() < m_Timestamps.capacity() && "pool is full");

	vkCmdWriteTimestamp(commandBuffer, stage, m_QueryPool, static_cast<uint32_t>(m_Timestamps.size()));
	if (!m_LabeledResults.contains(label))
	{
		m_LabeledResults[label].first  = static_cast<uint32_t>(m_Timestamps.size());
		m_LabeledResults[label].second = std::numeric_limits<uint32_t>::max();
	}
	else
	{
		assert(m_LabeledResults[label].second == std::numeric_limits<uint32_t>::max() &&
			   "label has been already recorded to, reset before making another recording");
		m_LabeledResults[label].second = static_cast<uint32_t>(m_Timestamps.size());
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
		for (auto const& [label, range]: m_LabeledResults)
		{
			auto const& [start, end] = range;
			assert(end != std::numeric_limits<uint32_t>::max() && "end of timestamp was not recorded");
			assert(end < m_Timestamps.size() && "invalid end index");

			outResult[label] = (m_Timestamps[end] - m_Timestamps[start]) * m_TimestampPeriod / static_cast<double>(std::micro::den);
		}
	}
}
