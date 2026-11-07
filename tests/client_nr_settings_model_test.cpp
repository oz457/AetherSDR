#include "TestSettingsProfile.h"
#include "core/AppSettings.h"
#include "models/ClientNrSettingsModel.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>

#include <cmath>
#include <cstdio>

using namespace AetherSDR;

namespace {

int g_failures = 0;

void expect(bool condition, const char* description)
{
    std::printf("%s %s\n", condition ? "[ OK ]" : "[FAIL]", description);
    if (!condition) {
        ++g_failures;
    }
}

bool nearlyEqual(float lhs, float rhs)
{
    return std::abs(lhs - rhs) < 1.0e-5f;
}

QJsonObject storedObject(AppSettings& settings)
{
    return QJsonDocument::fromJson(
               settings.value(QStringLiteral("ClientNR")).toString().toUtf8())
        .object();
}

float storedFloat(AppSettings& settings, const char* key)
{
    return static_cast<float>(
        storedObject(settings).value(QLatin1String(key)).toDouble(-1.0));
}

const char* const kLegacyKeys[] = {
    "NR4ReductionAmount", "NR4SmoothingFactor", "NR4WhiteningFactor",
    "NR4AdaptiveNoise", "NR4NoiseEstimationMethod", "NR4MaskingDepth",
    "NR4SuppressionStrength", "MnrStrength", "DfnrAttenLimit",
    "DfnrPostFilterBeta",
};

} // namespace

int main(int argc, char* argv[])
{
    TestSettingsProfile profile(
        QStringLiteral("aether-client-nr-settings-model-test"));
    QCoreApplication app(argc, argv);
    if (!profile.isValid()) {
        std::fprintf(stderr, "Unable to create isolated settings profile\n");
        return 1;
    }

    AppSettings& settings = AppSettings::instance();
    settings.load();

    // The defaults are the ones the flat keys were read with, so a user who
    // never touched a control hears the same receiver after upgrading.
    const ClientNrSettingsModel::Config d = ClientNrSettingsModel::defaults();
    expect(nearlyEqual(d.nr4ReductionDb, 10.0f), "NR4 reduction defaults to 10 dB");
    expect(nearlyEqual(d.nr4SmoothingPercent, 0.0f), "NR4 smoothing defaults to 0");
    expect(nearlyEqual(d.nr4WhiteningPercent, 0.0f), "NR4 whitening defaults to 0");
    expect(d.nr4AdaptiveNoise, "NR4 adaptive noise defaults on");
    expect(d.nr4NoiseMethod == 0, "NR4 noise method defaults to MMSE");
    expect(nearlyEqual(d.nr4MaskingDepth, 0.5f), "NR4 masking depth defaults to 0.50");
    expect(nearlyEqual(d.nr4SuppressionStrength, 0.5f),
           "NR4 suppression defaults to 0.50");
    expect(nearlyEqual(d.mnrStrength, 1.0f), "MNR strength defaults to full");
    expect(nearlyEqual(d.dfnrAttenLimitDb, 100.0f),
           "DFNR attenuation limit defaults to 100 dB");
    expect(nearlyEqual(d.dfnrPostFilterBeta, 0.0f),
           "DFNR post-filter beta defaults to off");

    // A profile from before the model: some flat keys set, the way the old
    // code wrote them (strings, "True"/"False"), one out of range. The model
    // is created after they are staged, as at a real first start.
    settings.setValue(QStringLiteral("NR4ReductionAmount"), QStringLiteral("55.0"));
    settings.setValue(QStringLiteral("NR4AdaptiveNoise"), QStringLiteral("False"));
    settings.setValue(QStringLiteral("NR4NoiseEstimationMethod"), QStringLiteral("2"));
    settings.setValue(QStringLiteral("NR4MaskingDepth"), QStringLiteral("0.75"));
    settings.setValue(QStringLiteral("MnrStrength"), QStringLiteral("0.40"));
    settings.setValue(QStringLiteral("DfnrAttenLimit"), QStringLiteral("60"));
    settings.setValue(QStringLiteral("DfnrPostFilterBeta"), QStringLiteral("0.10"));
    settings.save();

    ClientNrSettingsModel& model = ClientNrSettingsModel::instance();
    ClientNrSettingsModel::Config c = model.config();
    expect(nearlyEqual(c.nr4ReductionDb, 40.0f),
           "a legacy reduction above the slider range migrates clamped to 40 dB");
    expect(!c.nr4AdaptiveNoise, "legacy \"False\" adaptive noise migrates as off");
    expect(c.nr4NoiseMethod == 2, "legacy noise method migrates");
    expect(nearlyEqual(c.nr4MaskingDepth, 0.75f), "legacy masking depth migrates");
    expect(nearlyEqual(c.mnrStrength, 0.40f), "legacy MNR strength migrates");
    expect(nearlyEqual(c.dfnrAttenLimitDb, 60.0f),
           "legacy DFNR attenuation limit migrates");
    expect(nearlyEqual(c.dfnrPostFilterBeta, 0.10f),
           "legacy DFNR post-filter beta migrates");
    expect(nearlyEqual(c.nr4SmoothingPercent, 0.0f)
               && nearlyEqual(c.nr4WhiteningPercent, 0.0f)
               && nearlyEqual(c.nr4SuppressionStrength, 0.5f),
           "keys the profile never had migrate as their defaults");
    expect(c.version == 1, "the migrated config reports version 1");

    bool anyLegacyLeft = false;
    for (const char* key : kLegacyKeys) {
        anyLegacyLeft = anyLegacyLeft || settings.contains(QLatin1String(key));
    }
    expect(!anyLegacyLeft, "migration removes every flat key");
    expect(nearlyEqual(storedFloat(settings, "mnrStrength"), 0.40f)
               && storedObject(settings).value(QStringLiteral("version")).toInt() == 1,
           "migration writes one versioned ClientNR object");

    QSignalSpy changed(&model, &ClientNrSettingsModel::configChanged);
    model.setNr4ReductionDb(12.5f);
    expect(changed.count() == 1, "a real change broadcasts one refresh signal");
    expect(nearlyEqual(model.config().nr4ReductionDb, 12.5f), "the new value is held");
    expect(nearlyEqual(storedFloat(settings, "nr4ReductionDb"), 12.5f),
           "a change updates the stored object");

    model.setNr4ReductionDb(12.5f);
    expect(changed.count() == 1,
           "an unchanged value does not broadcast a duplicate refresh");

    // Clamping: the sliders cannot produce these, but a hand-edited store or
    // an automation client can.
    model.setNr4NoiseMethod(7);
    expect(model.config().nr4NoiseMethod == 2, "a noise method above Martin clamps to 2");
    model.setNr4NoiseMethod(-1);
    expect(model.config().nr4NoiseMethod == 0, "a negative noise method clamps to 0");
    model.setNr4MaskingDepth(1.5f);
    expect(nearlyEqual(model.config().nr4MaskingDepth, 1.0f), "masking depth clamps to 1");
    model.setNr4SmoothingPercent(250.0f);
    expect(nearlyEqual(model.config().nr4SmoothingPercent, 100.0f),
           "smoothing clamps to 100 percent");
    model.setMnrStrength(-0.2f);
    expect(nearlyEqual(model.config().mnrStrength, 0.0f), "MNR strength clamps to 0");
    model.setDfnrPostFilterBeta(0.5f);
    expect(nearlyEqual(model.config().dfnrPostFilterBeta,
                       ClientNrSettingsModel::kMaxDfnrPostFilterBeta),
           "DFNR beta clamps to the slider maximum");
    model.setDfnrAttenLimitDb(std::nanf(""));
    expect(nearlyEqual(model.config().dfnrAttenLimitDb, 100.0f),
           "a non-finite attenuation limit falls back to the default");

    // The engine reads the whole config when it builds a filter; it must see
    // every field the surfaces wrote.
    model.setNr4WhiteningPercent(30.0f);
    model.setNr4SuppressionStrength(0.8f);
    model.setNr4AdaptiveNoise(true);
    c = model.config();
    expect(nearlyEqual(c.nr4WhiteningPercent, 30.0f)
               && nearlyEqual(c.nr4SuppressionStrength, 0.8f)
               && c.nr4AdaptiveNoise
               && nearlyEqual(c.nr4ReductionDb, 12.5f),
           "per-field setters leave the other fields alone");

    std::printf("\n%d failure(s)\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
