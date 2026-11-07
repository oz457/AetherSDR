#pragma once

#include <QObject>

#include <mutex>

namespace AetherSDR {

// Process-wide owner for the parameters of the client-side NR4 (libspecbleach),
// MNR and DFNR (DeepFilterNet) stages. Like Nr2SettingsModel, it persists one
// versioned object instead of loose AppSettings keys, and every surface that
// edits these values (the Aether DSP panel, the right-click popups) writes
// here, so they cannot drift from one another. The engine reads the model
// when it builds a filter.
//
// Parameters only: which NR method is active stays where it is.
class ClientNrSettingsModel final : public QObject {
    Q_OBJECT

public:
    static constexpr int kConfigVersion = 1;

    struct Config {
        int version{kConfigVersion};

        // NR4, in the units the SpecbleachFilter setters take.
        float nr4ReductionDb{10.0f};         // 0 .. 40
        float nr4SmoothingPercent{0.0f};     // 0 .. 100
        float nr4WhiteningPercent{0.0f};     // 0 .. 100
        bool nr4AdaptiveNoise{true};
        int nr4NoiseMethod{0};               // 0 MMSE, 1 Brandt, 2 Martin
        float nr4MaskingDepth{0.50f};        // 0 .. 1
        float nr4SuppressionStrength{0.50f}; // 0 .. 1

        // MNR synthesis strength, 0 = bypass .. 1 = full.
        float mnrStrength{1.0f};

        // DFNR.
        float dfnrAttenLimitDb{100.0f};      // 0 .. 100
        float dfnrPostFilterBeta{0.0f};      // 0 .. 0.30

        bool operator==(const Config&) const = default;
    };

    static constexpr float kMaxNr4ReductionDb = 40.0f;
    static constexpr float kMaxDfnrAttenLimitDb = 100.0f;
    static constexpr float kMaxDfnrPostFilterBeta = 0.30f;

    static ClientNrSettingsModel& instance();
    static Config defaults();

    Config config() const;
    void setConfig(const Config& config);

    void setNr4ReductionDb(float db);
    void setNr4SmoothingPercent(float percent);
    void setNr4WhiteningPercent(float percent);
    void setNr4AdaptiveNoise(bool enabled);
    void setNr4NoiseMethod(int method);
    void setNr4MaskingDepth(float value);
    void setNr4SuppressionStrength(float value);
    void setMnrStrength(float value);
    void setDfnrAttenLimitDb(float db);
    void setDfnrPostFilterBeta(float beta);

signals:
    void configChanged();

private:
    ClientNrSettingsModel();

    static Config normalized(Config config);
    void load();
    void persist(const Config& config) const;

    mutable std::mutex m_mutex;
    Config m_config;
};

} // namespace AetherSDR
