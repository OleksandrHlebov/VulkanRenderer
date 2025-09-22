#ifndef VULKANRESEARCH_DELEGATE_H
#define VULKANRESEARCH_DELEGATE_H
// very simplified event system with direct delegate execution on trigger

#include <functional>
#include <vector>

template<typename... Args>
class Delegate final
{
public:
	Delegate(std::function<void(Args...)> action)
		: m_Action{ action } {}

	~Delegate() = default;

	Delegate(Delegate&&)                 = delete;
	Delegate(Delegate const&)            = delete;
	Delegate& operator=(Delegate&&)      = delete;
	Delegate& operator=(Delegate const&) = delete;

	void operator()(Args&&... args) const
	{
		m_Action(std::forward<Args>(args)...);
	}

private:
	std::function<void(Args...)> m_Action;
};

template<typename... Args>
class Event final
{
public:
	Event()  = default;
	~Event() = default;

	Event(Event&&)                 = default;
	Event(Event const&)            = delete;
	Event& operator=(Event&&)      = default;
	Event& operator=(Event const&) = delete;

	void Execute(Args&&... args) const
	{
		for (auto& action: m_Delegates)
			(*action)(std::forward<Args>(args)...);
	}

	void Bind(Delegate<Args...>& delegate)
	{
		m_Delegates.push_back(&delegate);
	}

	void Unbind(Delegate<Args...>& delegate)
	{
		std::erase(m_Delegates, &delegate);
	}

private:
	std::vector<Delegate<Args...>*> m_Delegates;
};

#endif //VULKANRESEARCH_DELEGATE_H
