#pragma once

#include "Types.h"
#include <mutex>
#include <vector>
#include <functional>
#include <string>

namespace Recorder::Core {

    struct StateChangedEvent {
        EngineState previousState;
        EngineState newState;
    };

    struct TelemetryUpdatedEvent {
        PerformanceTelemetry telemetry;
    };

    struct ErrorEvent {
        int32_t errorCode;
        std::wstring title;
        std::wstring description;
        bool isFatal;
    };

    struct RecordingFinishedEvent {
        std::wstring mkvFilePath;
        std::wstring mp4FilePath;
        bool remuxSuccess;
        uint64_t fileSizeBytes;
        uint64_t durationMs;
    };

    struct ReplaySavedEvent {
        std::wstring outputFilePath;
        uint64_t durationMs;
    };

    struct LowDiskSpaceEvent {
        uint64_t freeBytes;
        uint64_t thresholdBytes;
    };

    class EventBus {
    public:
        static EventBus& Instance();

        template <typename T>
        using Callback = std::function<void(const T&)>;

        // Subscription tokens
        using SubscriptionId = uint64_t;

        SubscriptionId SubscribeStateChanged(Callback<StateChangedEvent> callback);
        SubscriptionId SubscribeTelemetry(Callback<TelemetryUpdatedEvent> callback);
        SubscriptionId SubscribeError(Callback<ErrorEvent> callback);
        SubscriptionId SubscribeRecordingFinished(Callback<RecordingFinishedEvent> callback);
        SubscriptionId SubscribeReplaySaved(Callback<ReplaySavedEvent> callback);
        SubscriptionId SubscribeLowDiskSpace(Callback<LowDiskSpaceEvent> callback);

        void Unsubscribe(SubscriptionId id);

        void Publish(const StateChangedEvent& event);
        void Publish(const TelemetryUpdatedEvent& event);
        void Publish(const ErrorEvent& event);
        void Publish(const RecordingFinishedEvent& event);
        void Publish(const ReplaySavedEvent& event);
        void Publish(const LowDiskSpaceEvent& event);

    private:
        EventBus() = default;
        ~EventBus() = default;

        std::mutex m_mutex;
        SubscriptionId m_nextId = 1;

        std::vector<std::pair<SubscriptionId, Callback<StateChangedEvent>>> m_stateSubscribers;
        std::vector<std::pair<SubscriptionId, Callback<TelemetryUpdatedEvent>>> m_telemetrySubscribers;
        std::vector<std::pair<SubscriptionId, Callback<ErrorEvent>>> m_errorSubscribers;
        std::vector<std::pair<SubscriptionId, Callback<RecordingFinishedEvent>>> m_finishedSubscribers;
        std::vector<std::pair<SubscriptionId, Callback<ReplaySavedEvent>>> m_replaySubscribers;
        std::vector<std::pair<SubscriptionId, Callback<LowDiskSpaceEvent>>> m_diskSubscribers;
    };

} // namespace Recorder::Core
