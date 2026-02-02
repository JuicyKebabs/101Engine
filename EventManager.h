#pragma once

#include <functional>
#include <map>
#include <vector>
#include <memory>
#include <algorithm>
#include <cstdint>
#include "EventType.h"

class EventManager
{
public:

	EventManager() : nextId(1) {}

	~EventManager()
	{
		listeners.clear();
	}

	EventManager(const EventManager &) = delete;
	void operator=(const EventManager &) = delete;

	static EventManager *GetInstance()
	{
		static EventManager instance;
		return &instance;
	}

	template<typename T>
	std::uint64_t Subscribe(const EventType &eventType, std::function<void(std::shared_ptr<T>)> callback)
	{
		Listener wrapper = [callback](std::shared_ptr<void> data)
			{
				auto typed = std::static_pointer_cast<T>(data);
				callback(typed);
			};

		auto id = nextId++;
		listeners[eventType].push_back(ListenerItem{ id, wrapper });
		return id;
	}

	bool Unsubscribe(const EventType &eventType, std::uint64_t id)
	{
		auto &vec = listeners[eventType];
		auto it = std::remove_if(vec.begin(), vec.end(), [id](const ListenerItem &item) { return item.id == id; });
		if (it != vec.end())
		{
			vec.erase(it, vec.end());
			return true;
		}
		return false;
	}

	template<typename T>
	void TriggerEvent(const EventType &eventType, const T &arg)
	{
		auto shared = std::make_shared<T>(arg);
		auto copy = listeners[eventType];
		for (const auto &item : copy)
		{
			if (item.cb) item.cb(shared);
		}
	}

	void TriggerEvent(const EventType &eventType)
	{
		auto copy = listeners[eventType];
		for (const auto &item : copy)
		{
			if (item.cb) item.cb(nullptr);
		}
	}

private:
	using Listener = std::function<void(std::shared_ptr<void>)>;

	struct ListenerItem
	{
		std::uint64_t id;
		Listener cb;
	};

	std::map<EventType, std::vector<ListenerItem>> listeners;
	std::uint64_t nextId;
};