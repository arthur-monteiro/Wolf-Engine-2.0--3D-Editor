#pragma once

#include <cstdint>
#include <functional>

class Notifier
{
public:
	using Flags = uint32_t;

	void subscribe(const void* instance, const std::function<void(Flags)>& callback);
	void unsubscribe(const void* instance);

	bool isSubscribed(const void* instance) const;

protected:
	void notifySubscribers(Flags flags = 0) const;

private:
	struct Subscription
	{
		const void* instance;
		std::function<void(Flags)> callback;
	};
	std::vector<Subscription> m_subscriptions;
};

