#include "core/EventBus.h"
#include <algorithm>

namespace Recorder::Core {

    EventBus& EventBus::Instance() {
        static EventBus s_instance;
        return s_instance;
    }

    EventBus::SubscriptionId EventBus::SubscribeStateChanged(Callback<StateChangedEvent> callback) {
        std::lock_guard<std::mutex> lock(m_mutex);
        SubscriptionId id = m_nextId++;
        m_stateSubscribers.emplace_back(id, std::move(callback));
        return id;
    }

    EventBus::SubscriptionId EventBus::SubscribeTelemetry(Callback<TelemetryUpdatedEvent> callback) {
        std::lock_guard<std::mutex> lock(m_mutex);
        SubscriptionId id = m_nextId++;
        m_telemetrySubscribers.emplace_back(id, std::move(callback));
        return id;
    }

    EventBus::SubscriptionId EventBus::SubscribeError(Callback<ErrorEvent> callback) {
        std::lock_guard<std::mutex> lock(m_mutex);
        SubscriptionId id = m_nextId++;
        m_errorSubscribers.emplace_back(id, std::move(callback));
        return id;
    }

    EventBus::SubscriptionId EventBus::SubscribeRecordingFinished(Callback<RecordingFinishedEvent> callback) {
        std::lock_guard<std::mutex> lock(m_mutex);
        SubscriptionId id = m_nextId++;
        m_finishedSubscribers.emplace_back(id, std::move(callback));
        return id;
    }

    EventBus::SubscriptionId EventBus::SubscribeReplaySaved(Callback<ReplaySavedEvent> callback) {
        std::lock_guard<std::mutex> lock(m_mutex);
        SubscriptionId id = m_nextId++;
        m_replaySubscribers.emplace_back(id, std::move(callback));
        return id;
    }

    EventBus::SubscriptionId EventBus::SubscribeLowDiskSpace(Callback<LowDiskSpaceEvent> callback) {
        std::lock_guard<std::mutex> lock(m_mutex);
        SubscriptionId id = m_nextId++;
        m_diskSubscribers.emplace_back(id, std::move(callback));
        return id;
    }

    void EventBus::Unsubscribe(SubscriptionId id) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto removePredicate = [id](const auto& pair) { return pair.first == id; };

        m_stateSubscribers.erase(std::remove_if(m_stateSubscribers.begin(), m_stateSubscribers.end(), removePredicate), m_stateSubscribers.end());
        m_telemetrySubscribers.erase(std::remove_if(m_telemetrySubscribers.begin(), m_telemetrySubscribers.end(), removePredicate), m_telemetrySubscribers.end());
        m_errorSubscribers.erase(std::remove_if(m_errorSubscribers.begin(), m_errorSubscribers.end(), removePredicate), m_errorSubscribers.end());
        m_finishedSubscribers.erase(std::remove_if(m_finishedSubscribers.begin(), m_finishedSubscribers.end(), removePredicate), m_finishedSubscribers.end());
        m_replaySubscribers.erase(std::remove_if(m_replaySubscribers.begin(), m_replaySubscribers.end(), removePredicate), m_replaySubscribers.end());
        m_diskSubscribers.erase(std::remove_if(m_diskSubscribers.begin(), m_diskSubscribers.end(), removePredicate), m_diskSubscribers.end());
    }

    void EventBus::Publish(const StateChangedEvent& event) {
        std::vector<Callback<StateChangedEvent>> callbacks;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (const auto& pair : m_stateSubscribers) {
                callbacks.push_back(pair.second);
            }
        }
        for (const auto& cb : callbacks) {
            cb(event);
        }
    }

    void EventBus::Publish(const TelemetryUpdatedEvent& event) {
        std::vector<Callback<TelemetryUpdatedEvent>> callbacks;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (const auto& pair : m_telemetrySubscribers) {
                callbacks.push_back(pair.second);
            }
        }
        for (const auto& cb : callbacks) {
            cb(event);
        }
    }

    void EventBus::Publish(const ErrorEvent& event) {
        std::vector<Callback<ErrorEvent>> callbacks;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (const auto& pair : m_errorSubscribers) {
                callbacks.push_back(pair.second);
            }
        }
        for (const auto& cb : callbacks) {
            cb(event);
        }
    }

    void EventBus::Publish(const RecordingFinishedEvent& event) {
        std::vector<Callback<RecordingFinishedEvent>> callbacks;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (const auto& pair : m_finishedSubscribers) {
                callbacks.push_back(pair.second);
            }
        }
        for (const auto& cb : callbacks) {
            cb(event);
        }
    }

    void EventBus::Publish(const ReplaySavedEvent& event) {
        std::vector<Callback<ReplaySavedEvent>> callbacks;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (const auto& pair : m_replaySubscribers) {
                callbacks.push_back(pair.second);
            }
        }
        for (const auto& cb : callbacks) {
            cb(event);
        }
    }

    void EventBus::Publish(const LowDiskSpaceEvent& event) {
        std::vector<Callback<LowDiskSpaceEvent>> callbacks;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (const auto& pair : m_diskSubscribers) {
                callbacks.push_back(pair.second);
            }
        }
        for (const auto& cb : callbacks) {
            cb(event);
        }
    }

} // namespace Recorder::Core
