#include "Notifier.h"

void Notifier::subscribe(const void* instance, const std::function<void(Flags)>& callback)
{
	m_subscriptions.push_back({ instance, callback });
}

void Notifier::unsubscribe(const void* instance)
{
	for (int32_t i = static_cast<int32_t>(m_subscriptions.size()) - 1; i >= 0; --i)
	{
		if (m_subscriptions[i].instance == instance)
		{
			m_subscriptions.erase(m_subscriptions.begin() + i);
		}
	}
}

bool Notifier::isSubscribed(const void* instance) const
{
	for (const Subscription& subscription : m_subscriptions)
	{
		if (subscription.instance == instance)
			return true;
	}

	return false;
}

void Notifier::notifySubscribers(Flags flags) const
{
	for (const Subscription& subscription : m_subscriptions)
	{
		subscription.callback(flags);
	}
}
