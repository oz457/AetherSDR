#pragma once

// Test build only (tci-test-build): a plain float WAV writer for the HL2 TX
// tap. Buffered stdio on the I/O thread; 48 kHz stereo float is ~384 kB/s.

#include <cstdint>
#include <cstdio>
#include <string>

namespace AetherSDR::hl2 {

class Hl2TxWavTap {
public:
    ~Hl2TxWavTap() { close(); }

    bool open(const std::string& path, int channels, int rateHz)
    {
        close();
        m_file = std::fopen(path.c_str(), "wb");
        if (!m_file)
            return false;
        std::setvbuf(m_file, nullptr, _IOFBF, 1 << 16);
        m_channels = channels;
        m_rate = rateHz;
        m_frames = 0;
        writeHeader();
        return true;
    }

    [[nodiscard]] bool isOpen() const noexcept { return m_file != nullptr; }

    void write(const float* interleaved, std::size_t frames)
    {
        if (!m_file || frames == 0)
            return;
        std::fwrite(interleaved, sizeof(float) * static_cast<std::size_t>(m_channels), frames, m_file);
        m_frames += static_cast<std::uint32_t>(frames);
    }

    void close()
    {
        if (!m_file)
            return;
        std::fseek(m_file, 0, SEEK_SET);
        writeHeader();
        std::fclose(m_file);
        m_file = nullptr;
    }

private:
    void put32(std::uint32_t v) { std::fwrite(&v, 4, 1, m_file); }
    void put16(std::uint16_t v) { std::fwrite(&v, 2, 1, m_file); }
    void writeHeader()
    {
        const std::uint32_t data = m_frames * 4u * static_cast<std::uint32_t>(m_channels);
        std::fwrite("RIFF", 1, 4, m_file);
        put32(36u + data);
        std::fwrite("WAVEfmt ", 1, 8, m_file);
        put32(16);
        put16(3);  // IEEE float
        put16(static_cast<std::uint16_t>(m_channels));
        put32(static_cast<std::uint32_t>(m_rate));
        put32(static_cast<std::uint32_t>(m_rate * 4 * m_channels));
        put16(static_cast<std::uint16_t>(4 * m_channels));
        put16(32);
        std::fwrite("data", 1, 4, m_file);
        put32(data);
    }

    std::FILE* m_file = nullptr;
    int m_channels = 1;
    int m_rate = 48000;
    std::uint32_t m_frames = 0;
};

} // namespace AetherSDR::hl2
