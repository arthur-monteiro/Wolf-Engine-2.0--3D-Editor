#pragma once

#include <functional>

class Notifier
{
public:
	void subscribe(const void* instance, const std::function<void()>& callback);
	void unsubscribe(const void* instance);

protected:
	void notifySubscribers() const;

private:
	struct Subscription
	{
		const void* instance;
		std::function<void()> callback;
	};
	std::vector<Subscription> m_subscriptions;
};

