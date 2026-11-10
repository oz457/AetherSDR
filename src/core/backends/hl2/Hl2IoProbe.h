#pragma once

// Test build: where the HL2 I/O thread spends a stall.
//
// The I/O thread runs EP6 receive, every receiver's DSP, PureSignal, the TX
// modulator and the EP2 pacer. When it is late, the TX FIFO in the radio runs
// dry and the operator's audio breaks up on the air. The pacer stamps a
// heartbeat every tick; a monitor thread notices the heartbeat going stale,
// samples which stage the I/O thread is in, and after the stall logs how long
// it lasted, how much CPU the I/O thread used in that time (busy, or blocked /
// not scheduled), and the stages it was seen in.

#include <atomic>
#include <cstdint>

namespace AetherSDR::hl2 {

enum class IoStage : int {
    Loop = 0,      // event loop or untagged work
    Ep6Receive,    // datagram drain and parse
    RxDsp,         // a receiver's WDSP exchange
    RxSpectrum,    // a receiver's panadapter FFT
    PureSignal,
    TxModulator,   // Hl2TxDsp::processAudioBlock
    TxQueue,       // MetisClient::queueTxIq
    Ep2Send,       // pacer tick
    Watchdog,
    Count
};

struct Hl2IoProbeState {
    std::atomic<int> stage{0};
    std::atomic<std::int64_t> heartbeatNs{0};
};

Hl2IoProbeState& hl2IoProbe() noexcept;

// The pacer calls this every tick.
void hl2IoHeartbeat() noexcept;

// Called ON the I/O thread once it runs: raises it to MMCSS "Pro Audio" on
// Windows (unless AETHER_MMCSS=0), remembers it for the CPU reading, and
// starts the monitor once per process.
void hl2IoThreadStarted();

// Called on any stream thread that should run at MMCSS "Pro Audio" on Windows,
// as WDSP's own worker threads already do. No-op elsewhere.
void promoteCurrentThreadToProAudio(const char* name);

class IoStageScope {
public:
    explicit IoStageScope(IoStage stage) noexcept
        : m_previous(hl2IoProbe().stage.exchange(static_cast<int>(stage),
                                                 std::memory_order_relaxed))
    {
    }
    ~IoStageScope() { hl2IoProbe().stage.store(m_previous, std::memory_order_relaxed); }
    IoStageScope(const IoStageScope&) = delete;
    IoStageScope& operator=(const IoStageScope&) = delete;

private:
    int m_previous;
};

} // namespace AetherSDR::hl2
