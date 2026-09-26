#include "overlay/Telemetry.h"
#include <chrono>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace Recorder::Overlay {

    TelemetryCollector::TelemetryCollector() = default;

    void TelemetryCollector::RecordFrameCaptured() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_telemetry.totalFramesCaptured++;
    }

    void TelemetryCollector::RecordFrameEncoded(size_t packetSizeBytes) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_telemetry.totalFramesEncoded++;
        m_telemetry.bytesWritten += packetSizeBytes;
        m_bytesInLastWindow += packetSizeBytes;
    }

    void TelemetryCollector::RecordFrameDropped(const std::wstring& reason) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_telemetry.droppedFrames++;

        DroppedFrameRecord rec;
        rec.frameIndex = m_telemetry.totalFramesCaptured;
        rec.timestampMs = m_telemetry.recordingDurationMs;
        rec.reason = reason;
        m_droppedLogs.push_back(rec);
    }

    void TelemetryCollector::UpdateSystemMetrics(const std::wstring& recordingDriveRoot) {
        std::lock_guard<std::mutex> lock(m_mutex);

#if defined(_WIN32)
        // 1. Process CPU % Calculation
        FILETIME ftCreation, ftExit, ftKernel, ftUser;
        if (GetProcessTimes(GetCurrentProcess(), &ftCreation, &ftExit, &ftKernel, &ftUser)) {
            ULARGE_INTEGER ulKernel, ulUser;
            ulKernel.LowPart = ftKernel.dwLowDateTime;
            ulKernel.HighPart = ftKernel.dwHighDateTime;
            ulUser.LowPart = ftUser.dwLowDateTime;
            ulUser.HighPart = ftUser.dwHighDateTime;

            uint64_t currentProcTime = ulKernel.QuadPart + ulUser.QuadPart;

            FILETIME ftNow;
            GetSystemTimeAsFileTime(&ftNow);
            ULARGE_INTEGER ulNow;
            ulNow.LowPart = ftNow.dwLowDateTime;
            ulNow.HighPart = ftNow.dwHighDateTime;
            uint64_t currentSystemTime = ulNow.QuadPart;

            if (m_lastSystemTime > 0 && currentSystemTime > m_lastSystemTime) {
                uint64_t procDiff = currentProcTime - m_lastProcessCpuTime;
                uint64_t sysDiff = currentSystemTime - m_lastSystemTime;
                
                SYSTEM_INFO sysInfo;
                GetSystemInfo(&sysInfo);
                uint32_t numProcessors = sysInfo.dwNumberOfProcessors > 0 ? sysInfo.dwNumberOfProcessors : 1;

                m_telemetry.recorderCpuPercent = (static_cast<double>(procDiff) / static_cast<double>(sysDiff)) * 100.0 / numProcessors;
            }

            m_lastProcessCpuTime = currentProcTime;
            m_lastSystemTime = currentSystemTime;
        }

        // 2. Free Disk Space Check
        ULARGE_INTEGER freeBytesAvailable, totalNumberOfBytes, totalNumberOfFreeBytes;
        LPCWSTR driveDir = recordingDriveRoot.empty() ? nullptr : recordingDriveRoot.c_str();
        if (GetDiskFreeSpaceExW(driveDir, &freeBytesAvailable, &totalNumberOfBytes, &totalNumberOfFreeBytes)) {
            m_telemetry.freeDiskSpaceMb = freeBytesAvailable.QuadPart / (1024 * 1024);
        }

        // 3. Real-time Bitrate Calculation (rolling 1-second window)
        auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
        if (m_lastBitrateCalculationTime == 0) {
            m_lastBitrateCalculationTime = nowMs;
        } else if (nowMs - m_lastBitrateCalculationTime >= 1000) {
            double elapsedSec = static_cast<double>(nowMs - m_lastBitrateCalculationTime) / 1000.0;
            m_telemetry.currentBitrateKbps = (static_cast<double>(m_bytesInLastWindow) * 8.0) / (elapsedSec * 1000.0);
            m_bytesInLastWindow = 0;
            m_lastBitrateCalculationTime = nowMs;
        }
#endif
    }

    Core::PerformanceTelemetry TelemetryCollector::GetSnapshot() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_telemetry;
    }

    std::vector<DroppedFrameRecord> TelemetryCollector::GetDroppedFrameLogs() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_droppedLogs;
    }

    void TelemetryCollector::ClearLogs() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_droppedLogs.clear();
    }

} // namespace Recorder::Overlay
