#pragma once

#include "core/Types.h"
#include <mutex>
#include <vector>
#include <string>

namespace Recorder::Overlay {

    struct DroppedFrameRecord {
        uint64_t frameIndex;
        int64_t timestampMs;
        std::wstring reason;
    };

    class TelemetryCollector {
    public:
        TelemetryCollector();

        void RecordFrameCaptured();
        void RecordFrameEncoded(size_t packetSizeBytes);
        void RecordFrameDropped(const std::wstring& reason);

        void UpdateSystemMetrics(const std::wstring& recordingDriveRoot);

        Core::PerformanceTelemetry GetSnapshot() const;
        std::vector<DroppedFrameRecord> GetDroppedFrameLogs() const;
        void ClearLogs();

    private:
        mutable std::mutex m_mutex;
        Core::PerformanceTelemetry m_telemetry;
        std::vector<DroppedFrameRecord> m_droppedLogs;

        int64_t m_lastCpuCheckTime = 0;
        uint64_t m_lastProcessCpuTime = 0;
        uint64_t m_lastSystemTime = 0;
        uint64_t m_bytesInLastWindow = 0;
        int64_t m_lastBitrateCalculationTime = 0;
    };

} // namespace Recorder::Overlay
