#pragma once

#include <atomic>

// Test build only (branch test-7379): knobs the TCI applet sets live.
namespace AetherSDR::hl2::testknobs {

// Record every over as two WAV files in the logs folder: what enters the TXA
// modulator (mono, after mic gain) and what leaves it (I/Q, stereo). Set from
// the TCI applet; read on the HL2 I/O thread.
inline std::atomic<bool> txTapEnabled{false};

} // namespace AetherSDR::hl2::testknobs
