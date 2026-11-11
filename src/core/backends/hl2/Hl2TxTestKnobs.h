#pragma once

#include <atomic>

// Test build only (tci-test-build, never offered upstream): knobs the GUI sets
// and the HL2 TX DSP reads on its own thread. Plain atomics, no signal path,
// so the GUI does not need to reach the backend object.
namespace AetherSDR::hl2::testknobs {

// TXA leveler maximum gain in dB, set from the TCI applet's slider.
// Hl2TxDsp picks a change up on the next audio block it modulates.
inline std::atomic<double> levelerMaxGainDb{15.0};

// Record every over as two WAV files in the logs folder: what enters the TXA
// modulator (mono, after mic gain) and what leaves it (I/Q, stereo). Set from
// the TCI applet; read on the HL2 I/O thread.
inline std::atomic<bool> txTapEnabled{false};

} // namespace AetherSDR::hl2::testknobs
