#include "mux/MkvMuxer.h"
#include <cstring>
#include <iostream>

namespace Recorder::Mux {

    // EBML IDs
    constexpr uint32_t EBML_ID_HEADER = 0x1A45DFA3;
    constexpr uint32_t EBML_ID_VERSION = 0x4286;
    constexpr uint32_t EBML_ID_READ_VERSION = 0x42F7;
    constexpr uint32_t EBML_ID_MAX_ID_LENGTH = 0x42F2;
    constexpr uint32_t EBML_ID_MAX_SIZE_LENGTH = 0x42F3;
    constexpr uint32_t EBML_ID_DOCTYPE = 0x4282;
    constexpr uint32_t EBML_ID_DOCTYPE_VERSION = 0x4287;
    constexpr uint32_t EBML_ID_DOCTYPE_READ_VERSION = 0x4285;

    constexpr uint32_t MATROSKA_ID_SEGMENT = 0x18538067;
    constexpr uint32_t MATROSKA_ID_INFO = 0x1549A966;
    constexpr uint32_t MATROSKA_ID_TIMECODESCALE = 0x2AD7B1;
    constexpr uint32_t MATROSKA_ID_MUXINGAPP = 0x4D80;
    constexpr uint32_t MATROSKA_ID_WRITINGAPP = 0x5741;

    constexpr uint32_t MATROSKA_ID_TRACKS = 0x1654AE6B;
    constexpr uint32_t MATROSKA_ID_TRACKENTRY = 0xAE;
    constexpr uint32_t MATROSKA_ID_TRACKNUMBER = 0xD7;
    constexpr uint32_t MATROSKA_ID_TRACKUID = 0x73C5;
    constexpr uint32_t MATROSKA_ID_TRACKTYPE = 0x83;
    constexpr uint32_t MATROSKA_ID_CODECID = 0x86;
    constexpr uint32_t MATROSKA_ID_VIDEO = 0xE0;
    constexpr uint32_t MATROSKA_ID_PIXELWIDTH = 0xB0;
    constexpr uint32_t MATROSKA_ID_PIXELHEIGHT = 0xBA;
    constexpr uint32_t MATROSKA_ID_AUDIO = 0xE1;
    constexpr uint32_t MATROSKA_ID_CHANNELS = 0x9F;

    constexpr uint32_t MATROSKA_ID_CLUSTER = 0x1F43B675;
    constexpr uint32_t MATROSKA_ID_TIMECODE = 0xE7;
    constexpr uint32_t MATROSKA_ID_SIMPLEBLOCK = 0xA3;

    MkvMuxer::MkvMuxer() = default;

    MkvMuxer::~MkvMuxer() {
        Close();
    }

    bool MkvMuxer::IsOpen() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_isOpen;
    }

    uint64_t MkvMuxer::GetBytesWritten() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_bytesWritten;
    }

    uint64_t MkvMuxer::GetDurationMs() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_firstPtsHns < 0) return 0;
        int64_t diff = m_lastPtsHns - m_firstPtsHns;
        return (diff > 0) ? static_cast<uint64_t>(diff / 10'000) : 0;
    }

    std::wstring MkvMuxer::GetFilePath() const {
        return m_config.outputPath;
    }

    void MkvMuxer::WriteEbmlId(uint32_t id) {
        if (id > 0xFFFFFF) {
            uint8_t bytes[4] = {
                static_cast<uint8_t>((id >> 24) & 0xFF),
                static_cast<uint8_t>((id >> 16) & 0xFF),
                static_cast<uint8_t>((id >> 8) & 0xFF),
                static_cast<uint8_t>(id & 0xFF)
            };
            m_file.write(reinterpret_cast<const char*>(bytes), 4);
            m_bytesWritten += 4;
        } else if (id > 0xFFFF) {
            uint8_t bytes[3] = {
                static_cast<uint8_t>((id >> 16) & 0xFF),
                static_cast<uint8_t>((id >> 8) & 0xFF),
                static_cast<uint8_t>(id & 0xFF)
            };
            m_file.write(reinterpret_cast<const char*>(bytes), 3);
            m_bytesWritten += 3;
        } else if (id > 0xFF) {
            uint8_t bytes[2] = {
                static_cast<uint8_t>((id >> 8) & 0xFF),
                static_cast<uint8_t>(id & 0xFF)
            };
            m_file.write(reinterpret_cast<const char*>(bytes), 2);
            m_bytesWritten += 2;
        } else {
            uint8_t byte = static_cast<uint8_t>(id & 0xFF);
            m_file.write(reinterpret_cast<const char*>(&byte), 1);
            m_bytesWritten += 1;
        }
    }

    void MkvMuxer::WriteEbmlVint(uint64_t value) {
        if (value < 0x7F) {
            uint8_t b = static_cast<uint8_t>(0x80 | value);
            m_file.write(reinterpret_cast<const char*>(&b), 1);
            m_bytesWritten += 1;
        } else if (value < 0x3FFF) {
            uint8_t b[2] = {
                static_cast<uint8_t>(0x40 | ((value >> 8) & 0x3F)),
                static_cast<uint8_t>(value & 0xFF)
            };
            m_file.write(reinterpret_cast<const char*>(b), 2);
            m_bytesWritten += 2;
        } else if (value < 0x1FFFFF) {
            uint8_t b[3] = {
                static_cast<uint8_t>(0x20 | ((value >> 16) & 0x1F)),
                static_cast<uint8_t>((value >> 8) & 0xFF),
                static_cast<uint8_t>(value & 0xFF)
            };
            m_file.write(reinterpret_cast<const char*>(b), 3);
            m_bytesWritten += 3;
        } else if (value < 0x0FFFFFFF) {
            uint8_t b[4] = {
                static_cast<uint8_t>(0x10 | ((value >> 24) & 0x0F)),
                static_cast<uint8_t>((value >> 16) & 0xFF),
                static_cast<uint8_t>((value >> 8) & 0xFF),
                static_cast<uint8_t>(value & 0xFF)
            };
            m_file.write(reinterpret_cast<const char*>(b), 4);
            m_bytesWritten += 4;
        } else {
            uint8_t b[8] = {
                static_cast<uint8_t>(0x01),
                static_cast<uint8_t>((value >> 48) & 0xFF),
                static_cast<uint8_t>((value >> 40) & 0xFF),
                static_cast<uint8_t>((value >> 32) & 0xFF),
                static_cast<uint8_t>((value >> 24) & 0xFF),
                static_cast<uint8_t>((value >> 16) & 0xFF),
                static_cast<uint8_t>((value >> 8) & 0xFF),
                static_cast<uint8_t>(value & 0xFF)
            };
            m_file.write(reinterpret_cast<const char*>(b), 8);
            m_bytesWritten += 8;
        }
    }

    void MkvMuxer::WriteEbmlUint(uint32_t id, uint64_t val) {
        WriteEbmlId(id);
        if (val <= 0xFF) {
            WriteEbmlVint(1);
            uint8_t b = static_cast<uint8_t>(val);
            m_file.write(reinterpret_cast<const char*>(&b), 1);
            m_bytesWritten += 1;
        } else if (val <= 0xFFFF) {
            WriteEbmlVint(2);
            uint8_t b[2] = { static_cast<uint8_t>((val >> 8) & 0xFF), static_cast<uint8_t>(val & 0xFF) };
            m_file.write(reinterpret_cast<const char*>(b), 2);
            m_bytesWritten += 2;
        } else if (val <= 0xFFFFFFFF) {
            WriteEbmlVint(4);
            uint8_t b[4] = {
                static_cast<uint8_t>((val >> 24) & 0xFF),
                static_cast<uint8_t>((val >> 16) & 0xFF),
                static_cast<uint8_t>((val >> 8) & 0xFF),
                static_cast<uint8_t>(val & 0xFF)
            };
            m_file.write(reinterpret_cast<const char*>(b), 4);
            m_bytesWritten += 4;
        } else {
            WriteEbmlVint(8);
            for (int i = 7; i >= 0; --i) {
                uint8_t b = static_cast<uint8_t>((val >> (i * 8)) & 0xFF);
                m_file.write(reinterpret_cast<const char*>(&b), 1);
            }
            m_bytesWritten += 8;
        }
    }

    void MkvMuxer::WriteEbmlString(uint32_t id, const std::string& str) {
        WriteEbmlId(id);
        WriteEbmlVint(str.size());
        m_file.write(str.data(), str.size());
        m_bytesWritten += str.size();
    }

    void MkvMuxer::WriteEbmlHeader() {
        // Master EBML Header
        WriteEbmlId(EBML_ID_HEADER);
        WriteEbmlVint(31); // Size of header elements
        WriteEbmlUint(EBML_ID_VERSION, 1);
        WriteEbmlUint(EBML_ID_READ_VERSION, 1);
        WriteEbmlUint(EBML_ID_MAX_ID_LENGTH, 4);
        WriteEbmlUint(EBML_ID_MAX_SIZE_LENGTH, 8);
        WriteEbmlString(EBML_ID_DOCTYPE, "matroska");
        WriteEbmlUint(EBML_ID_DOCTYPE_VERSION, 4);
        WriteEbmlUint(EBML_ID_DOCTYPE_READ_VERSION, 2);
    }

    void MkvMuxer::WriteSegmentHeader() {
        WriteEbmlId(MATROSKA_ID_SEGMENT);
        // Unknown length vint: 0x01FFFFFFFFFFFFFF
        const uint8_t unknownSize[8] = { 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
        m_file.write(reinterpret_cast<const char*>(unknownSize), 8);
        m_bytesWritten += 8;

        // Info Element
        WriteEbmlId(MATROSKA_ID_INFO);
        WriteEbmlVint(50);
        WriteEbmlUint(MATROSKA_ID_TIMECODESCALE, 1'000'000); // 1ms
        WriteEbmlString(MATROSKA_ID_MUXINGAPP, "HighPerformanceRecorder");
        WriteEbmlString(MATROSKA_ID_WRITINGAPP, "HighPerformanceRecorder");
    }

    void MkvMuxer::WriteTracks() {
        WriteEbmlId(MATROSKA_ID_TRACKS);
        // Estimate Tracks size or use unknown size
        WriteEbmlVint(65);

        // Video Track (Track 1)
        WriteEbmlId(MATROSKA_ID_TRACKENTRY);
        WriteEbmlVint(60);
        WriteEbmlUint(MATROSKA_ID_TRACKNUMBER, 1);
        WriteEbmlUint(MATROSKA_ID_TRACKUID, 1);
        WriteEbmlUint(MATROSKA_ID_TRACKTYPE, 1); // Video

        std::string codecId = "V_MPEG4/ISO/AVC";
        if (m_config.video.codec == Core::VideoCodec::HEVC) {
            codecId = "V_MPEGH/ISO/HEVC";
        } else if (m_config.video.codec == Core::VideoCodec::AV1) {
            codecId = "V_AV1";
        }
        WriteEbmlString(MATROSKA_ID_CODECID, codecId);

        // Video settings
        WriteEbmlId(MATROSKA_ID_VIDEO);
        WriteEbmlVint(10);
        WriteEbmlUint(MATROSKA_ID_PIXELWIDTH, m_config.video.width);
        WriteEbmlUint(MATROSKA_ID_PIXELHEIGHT, m_config.video.height);
    }

    bool MkvMuxer::Open(const MuxerConfig& config) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_config = config;

        m_file.open(m_config.outputPath, std::ios::binary | std::ios::trunc);
        if (!m_file.is_open()) return false;

        m_bytesWritten = 0;
        m_firstPtsHns = -1;
        m_lastPtsHns = 0;
        m_clusterOpen = false;

        WriteEbmlHeader();
        WriteSegmentHeader();
        WriteTracks();

        m_isOpen = true;
        return true;
    }

    bool MkvMuxer::WritePacket(const Core::MediaPacket& packet) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_isOpen || packet.data.empty()) return false;

        if (m_firstPtsHns < 0) {
            m_firstPtsHns = packet.ptsHns;
        }
        m_lastPtsHns = packet.ptsHns;

        int64_t currentPtsMs = (packet.ptsHns - m_firstPtsHns) / 10'000;
        if (currentPtsMs < 0) currentPtsMs = 0;

        // Open a new cluster on keyframes or if cluster has exceeded 1000ms
        bool isKey = (packet.type == Core::PacketType::VideoKeyframe || packet.isKeyframe);
        if (!m_clusterOpen || isKey || (currentPtsMs - m_clusterTimecodeMs > 1000)) {
            m_clusterTimecodeMs = currentPtsMs;
            m_clusterOpen = true;

            WriteEbmlId(MATROSKA_ID_CLUSTER);
            const uint8_t unknownSize[8] = { 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
            m_file.write(reinterpret_cast<const char*>(unknownSize), 8);
            m_bytesWritten += 8;

            WriteEbmlUint(MATROSKA_ID_TIMECODE, static_cast<uint64_t>(m_clusterTimecodeMs));
        }

        // Write SimpleBlock (ID 0xA3)
        // Header: TrackNumber (1 byte: 0x81 for track 1, 0x82 for track 2)
        // Relative Timecode (int16_t big endian)
        // Flags (1 byte: 0x80 for Keyframe, 0x00 for Delta)
        int16_t relTimecode = static_cast<int16_t>(currentPtsMs - m_clusterTimecodeMs);
        uint8_t trackNumByte = (packet.type == Core::PacketType::AudioFrame) ? 0x82 : 0x81;
        uint8_t flags = isKey ? 0x80 : 0x00;

        uint64_t blockPayloadSize = 4 + packet.data.size();
        WriteEbmlId(MATROSKA_ID_SIMPLEBLOCK);
        WriteEbmlVint(blockPayloadSize);

        uint8_t header[4] = {
            trackNumByte,
            static_cast<uint8_t>((relTimecode >> 8) & 0xFF),
            static_cast<uint8_t>(relTimecode & 0xFF),
            flags
        };
        m_file.write(reinterpret_cast<const char*>(header), 4);
        m_file.write(reinterpret_cast<const char*>(packet.data.data()), packet.data.size());
        m_bytesWritten += (4 + packet.data.size());

        return true;
    }

    bool MkvMuxer::Close() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_isOpen) return true;

        m_file.flush();
        m_file.close();
        m_isOpen = false;
        return true;
    }

} // namespace Recorder::Mux
