#ifndef DELETIONQUEUE_H
#define DELETIONQUEUE_H
#include <functional>
#include <stack>

namespace vkc
{
	class DeletionQueue final
	{
	public:
		using Deleter = std::function<void()>;
		DeletionQueue() = default;

		~DeletionQueue()
		{
			Flush();
		}

		DeletionQueue(DeletionQueue&&)                 = delete;
		DeletionQueue(DeletionQueue const&)            = delete;
		DeletionQueue& operator=(DeletionQueue const&) = delete;
		DeletionQueue& operator=(DeletionQueue&&)      = delete;

		void Push(Deleter&& deleter)
		{
			m_Deleters.push(deleter);
		}

		void Flush()
		{
			while (!m_Deleters.empty())
			{
				m_Deleters.top()();
				m_Deleters.pop();
			}
		}

	private:
		std::stack<Deleter> m_Deleters;
	};
}

#endif //DELETIONQUEUE_H
