#pragma once

#include "MuxTypes.h"
#include "../core/Types.h"
#include <fstream>
#include <mutex>
#include <vector>

namespace Recorder::Mux {

    class MkvMuxer {
    public:
        MkvMuxer();
        ~MkvMuxer();

        bool Open(const MuxerConfig& config);
        bool WritePacket(const Core::MediaPacket& packet);
        bool Close();

        bool IsOpen() const;
        uint64_t GetBytesWritten() const;
        uint64_t GetDurationMs() const;
        std::wstring GetFilePath() const;

    private:
        void WriteEbmlHeader();
        void WriteSegmentHeader();
        void WriteTracks();
        void FlushCurrentCluster();

        // EBML Helper serialization routines
        void WriteEbmlId(uint32_t id);
        void WriteEbmlVint(uint64_t value);
        void WriteEbmlUint(uint32_t id, uint64_t val);
        void WriteEbmlString(uint32_t id, const std::string& str);
        void WriteEbmlBinary(uint32_t id, const uint8_t* data, size_t size);

        mutable std::mutex m_mutex;
        std::ofstream m_file;
        MuxerConfig m_config;
        
        bool m_isOpen = false;
        uint64_t m_bytesWritten = 0;
        int64_t m_firstPtsHns = -1;
        int64_t m_lastPtsHns = 0;

        // Active cluster state
        int64_t m_clusterTimecodeMs = 0;
        bool m_clusterOpen = false;
        std::vector<uint8_t> m_clusterBuffer;
    };

} // namespace Recorder::Mux
