#include "core/backends/hl2/Hl2IoProbe.h"

#include <QLoggingCategory>
#include <QString>
#include <QtGlobal>

#include <array>
#include <chrono>
#include <thread>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif
#endif

// Same category as the TX modulator, so these lines sit with the TX audio
// diagnostics in the log.
static Q_LOGGING_CATEGORY(lcIoProbe, "aether.hl2.tx")

namespace AetherSDR::hl2 {

namespace {

std::int64_t steadyNs() noexcept
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

const char* stageName(int stage) noexcept
{
    switch (static_cast<IoStage>(stage)) {
    case IoStage::Loop:        return "loop/other";
    case IoStage::Ep6Receive:  return "ep6-receive";
    case IoStage::RxDsp:       return "rx-dsp";
    case IoStage::RxSpectrum:  return "rx-spectrum";
    case IoStage::PureSignal:  return "puresignal";
    case IoStage::TxModulator: return "tx-modulator";
    case IoStage::TxQueue:     return "tx-queue";
    case IoStage::Ep2Send:     return "ep2-send";
    case IoStage::Watchdog:    return "watchdog";
    case IoStage::Count:       break;
    }
    return "?";
}

std::atomic<bool> g_monitorStarted{false};

#ifdef _WIN32
bool mmcssDisabled()
{
    return qEnvironmentVariable("AETHER_MMCSS") == QLatin1String("0");
}

std::atomic<HANDLE> g_ioThread{nullptr};

// Kernel + user time of the I/O thread, in ms; -1 when unknown.
double ioThreadCpuMs() noexcept
{
    const HANDLE thread = g_ioThread.load(std::memory_order_acquire);
    if (!thread)
        return -1.0;
    FILETIME created, exited, kernel, user;
    if (!GetThreadTimes(thread, &created, &exited, &kernel, &user))
        return -1.0;
    const auto ticks = [](const FILETIME& t) {
        return (static_cast<std::uint64_t>(t.dwHighDateTime) << 32) | t.dwLowDateTime;
    };
    return static_cast<double>(ticks(kernel) + ticks(user)) / 10000.0;   // 100 ns -> ms
}
#else
double ioThreadCpuMs() noexcept { return -1.0; }
#endif

void monitorLoop()
{
#ifdef _WIN32
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr,
                                          CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                          TIMER_ALL_ACCESS);
    const auto pause = [timer] {
        if (timer) {
            LARGE_INTEGER due;
            due.QuadPart = -10000;   // 1 ms, relative
            if (SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE)) {
                WaitForSingleObject(timer, 100);
                return;
            }
        }
        Sleep(1);
    };
#else
    const auto pause = [] { std::this_thread::sleep_for(std::chrono::milliseconds(1)); };
#endif

    constexpr std::int64_t kStaleNs = 25'000'000;        // pacer ticks every 2 ms
    constexpr std::int64_t kReportNs = 40'000'000;       // log stalls from 40 ms
    constexpr std::int64_t kLinkDownNs = 5'000'000'000;  // longer = stopped, not stalled

    Hl2IoProbeState& probe = hl2IoProbe();
    std::int64_t lastBeat = 0;
    double cpuAtLastBeat = -1.0;
    std::array<int, static_cast<std::size_t>(IoStage::Count)> samples{};
    int sampleTotal = 0;
    bool stale = false;

    for (;;) {
        pause();
        const std::int64_t beat = probe.heartbeatNs.load(std::memory_order_acquire);
        if (beat == 0)
            continue;
        const std::int64_t now = steadyNs();

        if (beat != lastBeat) {
            // The I/O thread is ticking again (or still).
            if (stale && lastBeat != 0) {
                const std::int64_t gapNs = beat - lastBeat;
                if (gapNs >= kReportNs && gapNs < kLinkDownNs) {
                    const double cpuNow = ioThreadCpuMs();
                    QString where;
                    for (std::size_t s = 0; s < samples.size(); ++s) {
                        if (samples[s] == 0)
                            continue;
                        if (!where.isEmpty())
                            where += QStringLiteral(", ");
                        where += QStringLiteral("%1 %2%")
                                     .arg(QLatin1String(stageName(static_cast<int>(s))))
                                     .arg(100 * samples[s] / qMax(1, sampleTotal));
                    }
                    const double cpuMs = (cpuNow >= 0.0 && cpuAtLastBeat >= 0.0)
                        ? cpuNow - cpuAtLastBeat : -1.0;
                    const double gapMs = static_cast<double>(gapNs) / 1e6;
                    const char* verdict = cpuMs < 0.0 ? "cpu unknown"
                        : (cpuMs >= 0.6 * gapMs ? "BUSY: the thread was computing"
                                                : "WAITING: blocked on a lock or not scheduled");
                    qCWarning(lcIoProbe).nospace()
                        << "HL2 io stall: " << qRound(gapMs) << " ms between pacer ticks; "
                        << "io-thread CPU in that time " << qRound(cpuMs) << " ms (" << verdict
                        << "); seen in: " << qPrintable(where)
                        << " (" << sampleTotal << " samples)";
                }
            }
            stale = false;
            samples.fill(0);
            sampleTotal = 0;
            lastBeat = beat;
            cpuAtLastBeat = ioThreadCpuMs();
            continue;
        }

        if (now - beat >= kStaleNs && now - beat < kLinkDownNs) {
            stale = true;
            const int stage = probe.stage.load(std::memory_order_relaxed);
            if (stage >= 0 && stage < static_cast<int>(IoStage::Count)) {
                ++samples[static_cast<std::size_t>(stage)];
                ++sampleTotal;
            }
        }
    }
}

} // namespace

Hl2IoProbeState& hl2IoProbe() noexcept
{
    static Hl2IoProbeState state;
    return state;
}

void hl2IoHeartbeat() noexcept
{
    hl2IoProbe().heartbeatNs.store(steadyNs(), std::memory_order_release);
}

void promoteCurrentThreadToProAudio(const char* name)
{
#ifdef _WIN32
    if (mmcssDisabled()) {
        qCInfo(lcIoProbe) << "MMCSS:" << name << "left at normal priority (AETHER_MMCSS=0)";
        return;
    }
    using SetFn = HANDLE(WINAPI*)(LPCWSTR, LPDWORD);
    using PrioFn = BOOL(WINAPI*)(HANDLE, int);
    static HMODULE avrt = LoadLibraryW(L"avrt.dll");
    const auto set = avrt ? reinterpret_cast<SetFn>(
                                GetProcAddress(avrt, "AvSetMmThreadCharacteristicsW"))
                          : nullptr;
    const auto prio = avrt ? reinterpret_cast<PrioFn>(
                                 GetProcAddress(avrt, "AvSetMmThreadPriority"))
                           : nullptr;
    DWORD taskIndex = 0;
    const HANDLE task = set ? set(L"Pro Audio", &taskIndex) : nullptr;
    if (task) {
        // 2 = AVRT_PRIORITY_CRITICAL, what WDSP's wdspmain() asks for: the
        // thread that feeds the WDSP workers must not sit below them.
        if (prio)
            prio(task, 2);
        qCInfo(lcIoProbe) << "MMCSS:" << name << "runs as \"Pro Audio\", critical (task"
                          << taskIndex << ")";
    } else {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
        qCWarning(lcIoProbe) << "MMCSS:" << name << "refused (error" << GetLastError()
                             << "), using THREAD_PRIORITY_HIGHEST instead";
    }
#else
    Q_UNUSED(name);
#endif
}

void hl2IoThreadStarted()
{
    promoteCurrentThreadToProAudio("hl2-io");
#ifdef _WIN32
    const HANDLE self = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentThreadId());
    // The previous backend's handle is left open: one handle per reconnect.
    if (self)
        g_ioThread.store(self, std::memory_order_release);
#endif
    if (!g_monitorStarted.exchange(true)) {
        // Detached: it only reads atomics, and a joinable static thread would
        // terminate the process at exit.
        std::thread(monitorLoop).detach();
        qCInfo(lcIoProbe) << "HL2 io stall monitor running (reports stalls of 40 ms and more)";
    }
}

} // namespace AetherSDR::hl2
