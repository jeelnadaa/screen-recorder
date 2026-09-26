#include "replay/CircularBuffer.h"
#include <algorithm>

namespace Recorder::Replay {

    CircularBuffer::CircularBuffer() = default;

    void CircularBuffer::Configure(uint32_t durationSeconds, uint32_t maxRamMb) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_durationSeconds = durationSeconds;
        m_maxBytes = static_cast<size_t>(maxRamMb) * 1024 * 1024;
        PruneOldPackets();
    }

    void CircularBuffer::Reset() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_packets.clear();
        m_currentBytes = 0;
    }

    void CircularBuffer::PushPacket(Core::MediaPacket&& packet) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_currentBytes += packet.data.size() + sizeof(Core::MediaPacket);
        m_packets.push_back(std::move(packet));
        PruneOldPackets();
    }

    void CircularBuffer::PruneOldPackets() {
        if (m_packets.empty()) return;

        int64_t latestPts = m_packets.back().ptsHns;
        int64_t windowHns = static_cast<int64_t>(m_durationSeconds) * 10'000'000LL; // 100ns units

        // Find the boundary where packets are older than the time window or memory limit is exceeded
        while (!m_packets.empty()) {
            bool timeExceeded = (latestPts - m_packets.front().ptsHns) > windowHns;
            bool memExceeded = m_currentBytes > m_maxBytes;

            if (!timeExceeded && !memExceeded) {
                break;
            }

            // Find next keyframe to ensure the stream starts with a keyframe
            // Don't prune if only 1 keyframe remains
            size_t nextKeyframeIdx = 0;
            bool foundNextKeyframe = false;
            for (size_t i = 1; i < m_packets.size(); ++i) {
                if (m_packets[i].type == Core::PacketType::VideoKeyframe || m_packets[i].isKeyframe) {
                    nextKeyframeIdx = i;
                    foundNextKeyframe = true;
                    break;
                }
            }

            if (!foundNextKeyframe) {
                // Cannot prune further without discarding our only remaining keyframe
                break;
            }

            // Remove packets up to nextKeyframeIdx
            for (size_t i = 0; i < nextKeyframeIdx; ++i) {
                m_currentBytes -= (m_packets.front().data.size() + sizeof(Core::MediaPacket));
                m_packets.pop_front();
            }
        }
    }

    std::vector<Core::MediaPacket> CircularBuffer::SnapshotPackets() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<Core::MediaPacket> result;
        result.reserve(m_packets.size());

        // Locate the first keyframe
        auto firstKeyframe = std::find_if(m_packets.begin(), m_packets.end(), [](const Core::MediaPacket& p) {
            return p.type == Core::PacketType::VideoKeyframe || p.isKeyframe;
        });

        for (auto it = firstKeyframe; it != m_packets.end(); ++it) {
            result.push_back(*it);
        }

        return result;
    }

    size_t CircularBuffer::GetPacketCount() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_packets.size();
    }

    size_t CircularBuffer::GetMemoryUsageBytes() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_currentBytes;
    }

    uint64_t CircularBuffer::GetBufferedDurationMs() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_packets.empty()) return 0;
        int64_t diffHns = m_packets.back().ptsHns - m_packets.front().ptsHns;
        return (diffHns > 0) ? static_cast<uint64_t>(diffHns / 10'000) : 0;
    }

} // namespace Recorder::Replay
