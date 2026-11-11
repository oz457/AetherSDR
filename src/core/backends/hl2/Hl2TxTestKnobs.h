#pragma once

#include <atomic>

// Test build only (tci-test-build, never offered upstream): knobs the GUI sets
// and the HL2 TX DSP reads on its own thread. Plain atomics, no signal path,
// so the GUI does not need to reach the backend object.
namespace AetherSDR::hl2::testknobs {

// TXA leveler maximum gain in dB, set from the TCI applet's slider.
// Hl2TxDsp picks a change up on the next audio block it modulates.
inline std::atomic<double> levelerMaxGainDb{15.0};

} // namespace AetherSDR::hl2::testknobs
