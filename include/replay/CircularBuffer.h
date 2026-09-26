#pragma once

#include "../core/Types.h"
#include <vector>
#include <deque>
#include <mutex>
#include <cstdint>
#include <string>

namespace Recorder::Replay {

    class CircularBuffer {
    public:
        CircularBuffer();
        ~CircularBuffer() = default;

        void Configure(uint32_t durationSeconds, uint32_t maxRamMb);
        void Reset();

        // Hot-path method to add encoded packet
        void PushPacket(Core::MediaPacket&& packet);

        // Snapshot all packets starting from the earliest valid keyframe within duration window
        std::vector<Core::MediaPacket> SnapshotPackets() const;

        size_t GetPacketCount() const;
        size_t GetMemoryUsageBytes() const;
        uint64_t GetBufferedDurationMs() const;

    private:
        void PruneOldPackets();

        mutable std::mutex m_mutex;
        std::deque<Core::MediaPacket> m_packets;
        
        uint32_t m_durationSeconds = 60;
        size_t m_maxBytes = 1024 * 1024 * 1024; // 1 GB default
        size_t m_currentBytes = 0;
    };

} // namespace Recorder::Replay
