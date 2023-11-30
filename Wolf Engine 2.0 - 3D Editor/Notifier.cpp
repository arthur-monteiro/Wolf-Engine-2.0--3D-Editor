#include "Notifier.h"

void Notifier::subscribe(const void* instance, const std::function<void()>& callback)
{
	m_subscriptions.push_back({ instance, callback });
}

void Notifier::unsubscribe(const void* instance)
{
	for (int32_t i = static_cast<int32_t>(m_subscriptions.size()); i >= 0; ++i)
	{
		if (m_subscriptions[i].instance == instance)
		{
			m_subscriptions.erase(m_subscriptions.begin() + i);
		}
	}
}

void Notifier::notifySubscribers() const
{
	for (const Subscription& subscription : m_subscriptions)
	{
		subscription.callback();
	}
}
