#include "models/ClientNrSettingsModel.h"

#include "core/AppSettings.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>

#include <algorithm>
#include <array>
#include <cmath>

namespace AetherSDR {

namespace {

Q_LOGGING_CATEGORY(lcClientNrSettings, "aether.clientnr.settings")

const QString kRootKey = QStringLiteral("ClientNR");

// The flat keys these values lived under before the model. They are read
// once, folded into the object and removed.
const QString kLegacyNr4Reduction = QStringLiteral("NR4ReductionAmount");
const QString kLegacyNr4Smoothing = QStringLiteral("NR4SmoothingFactor");
const QString kLegacyNr4Whitening = QStringLiteral("NR4WhiteningFactor");
const QString kLegacyNr4Adaptive = QStringLiteral("NR4AdaptiveNoise");
const QString kLegacyNr4Method = QStringLiteral("NR4NoiseEstimationMethod");
const QString kLegacyNr4Masking = QStringLiteral("NR4MaskingDepth");
const QString kLegacyNr4Suppression = QStringLiteral("NR4SuppressionStrength");
const QString kLegacyMnrStrength = QStringLiteral("MnrStrength");
const QString kLegacyDfnrAtten = QStringLiteral("DfnrAttenLimit");
const QString kLegacyDfnrBeta = QStringLiteral("DfnrPostFilterBeta");

const std::array<QString, 10> kLegacyKeys = {
    kLegacyNr4Reduction,
    kLegacyNr4Smoothing,
    kLegacyNr4Whitening,
    kLegacyNr4Adaptive,
    kLegacyNr4Method,
    kLegacyNr4Masking,
    kLegacyNr4Suppression,
    kLegacyMnrStrength,
    kLegacyDfnrAtten,
    kLegacyDfnrBeta,
};

bool settingIsTrue(const QVariant& value)
{
    return value.toString().compare(QLatin1String("True"),
                                    Qt::CaseInsensitive) == 0
        || value.toBool();
}

float clampedOr(float value, float lo, float hi, float fallback)
{
    return std::isfinite(value) ? std::clamp(value, lo, hi) : fallback;
}

QJsonObject toJson(const ClientNrSettingsModel::Config& config)
{
    return QJsonObject{
        {QStringLiteral("version"), config.version},
        {QStringLiteral("nr4ReductionDb"), config.nr4ReductionDb},
        {QStringLiteral("nr4SmoothingPercent"), config.nr4SmoothingPercent},
        {QStringLiteral("nr4WhiteningPercent"), config.nr4WhiteningPercent},
        {QStringLiteral("nr4AdaptiveNoise"), config.nr4AdaptiveNoise},
        {QStringLiteral("nr4NoiseMethod"), config.nr4NoiseMethod},
        {QStringLiteral("nr4MaskingDepth"), config.nr4MaskingDepth},
        {QStringLiteral("nr4SuppressionStrength"), config.nr4SuppressionStrength},
        {QStringLiteral("mnrStrength"), config.mnrStrength},
        {QStringLiteral("dfnrAttenLimitDb"), config.dfnrAttenLimitDb},
        {QStringLiteral("dfnrPostFilterBeta"), config.dfnrPostFilterBeta},
    };
}

float floatField(const QJsonObject& object, const char* key, float fallback)
{
    return static_cast<float>(
        object.value(QLatin1String(key)).toDouble(fallback));
}

ClientNrSettingsModel::Config fromJson(const QJsonObject& object)
{
    ClientNrSettingsModel::Config c = ClientNrSettingsModel::defaults();
    c.nr4ReductionDb = floatField(object, "nr4ReductionDb", c.nr4ReductionDb);
    c.nr4SmoothingPercent =
        floatField(object, "nr4SmoothingPercent", c.nr4SmoothingPercent);
    c.nr4WhiteningPercent =
        floatField(object, "nr4WhiteningPercent", c.nr4WhiteningPercent);
    c.nr4AdaptiveNoise = object.value(QStringLiteral("nr4AdaptiveNoise"))
                             .toBool(c.nr4AdaptiveNoise);
    c.nr4NoiseMethod = object.value(QStringLiteral("nr4NoiseMethod"))
                           .toInt(c.nr4NoiseMethod);
    c.nr4MaskingDepth = floatField(object, "nr4MaskingDepth", c.nr4MaskingDepth);
    c.nr4SuppressionStrength =
        floatField(object, "nr4SuppressionStrength", c.nr4SuppressionStrength);
    c.mnrStrength = floatField(object, "mnrStrength", c.mnrStrength);
    c.dfnrAttenLimitDb =
        floatField(object, "dfnrAttenLimitDb", c.dfnrAttenLimitDb);
    c.dfnrPostFilterBeta =
        floatField(object, "dfnrPostFilterBeta", c.dfnrPostFilterBeta);
    return c;
}

QString compactJson(const ClientNrSettingsModel::Config& config)
{
    return QString::fromUtf8(
        QJsonDocument(toJson(config)).toJson(QJsonDocument::Compact));
}

} // namespace

ClientNrSettingsModel& ClientNrSettingsModel::instance()
{
    static ClientNrSettingsModel model;
    return model;
}

ClientNrSettingsModel::Config ClientNrSettingsModel::defaults()
{
    return Config{};
}

ClientNrSettingsModel::ClientNrSettingsModel()
{
    load();
}

ClientNrSettingsModel::Config ClientNrSettingsModel::normalized(Config c)
{
    const Config d = defaults();
    c.version = kConfigVersion;
    c.nr4ReductionDb =
        clampedOr(c.nr4ReductionDb, 0.0f, kMaxNr4ReductionDb, d.nr4ReductionDb);
    c.nr4SmoothingPercent =
        clampedOr(c.nr4SmoothingPercent, 0.0f, 100.0f, d.nr4SmoothingPercent);
    c.nr4WhiteningPercent =
        clampedOr(c.nr4WhiteningPercent, 0.0f, 100.0f, d.nr4WhiteningPercent);
    c.nr4NoiseMethod = std::clamp(c.nr4NoiseMethod, 0, 2);
    c.nr4MaskingDepth =
        clampedOr(c.nr4MaskingDepth, 0.0f, 1.0f, d.nr4MaskingDepth);
    c.nr4SuppressionStrength = clampedOr(
        c.nr4SuppressionStrength, 0.0f, 1.0f, d.nr4SuppressionStrength);
    c.mnrStrength = clampedOr(c.mnrStrength, 0.0f, 1.0f, d.mnrStrength);
    c.dfnrAttenLimitDb = clampedOr(
        c.dfnrAttenLimitDb, 0.0f, kMaxDfnrAttenLimitDb, d.dfnrAttenLimitDb);
    c.dfnrPostFilterBeta = clampedOr(
        c.dfnrPostFilterBeta, 0.0f, kMaxDfnrPostFilterBeta, d.dfnrPostFilterBeta);
    return c;
}

ClientNrSettingsModel::Config ClientNrSettingsModel::config() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config;
}

void ClientNrSettingsModel::load()
{
    AppSettings& settings = AppSettings::instance();
    const QString raw = settings.value(kRootKey, QString{}).toString();
    if (!raw.isEmpty()) {
        QJsonParseError error;
        const QJsonDocument document =
            QJsonDocument::fromJson(raw.toUtf8(), &error);
        if (error.error == QJsonParseError::NoError && document.isObject()) {
            const Config loaded = normalized(fromJson(document.object()));
            const QString normalizedJson = compactJson(loaded);
            bool changed = normalizedJson != raw;
            // The object wins over any flat key left behind, e.g. by an
            // older build run against the same profile.
            for (const QString& key : kLegacyKeys) {
                if (settings.contains(key)) {
                    settings.remove(key);
                    changed = true;
                }
            }
            if (changed) {
                settings.setValue(kRootKey, normalizedJson);
                settings.save();
            }
            std::lock_guard<std::mutex> lock(m_mutex);
            m_config = loaded;
            return;
        }
        qCWarning(lcClientNrSettings)
            << "Ignoring malformed client NR settings object:"
            << error.errorString();
        settings.remove(kRootKey);
        settings.save();
    }

    const bool hasLegacy = std::any_of(
        kLegacyKeys.cbegin(), kLegacyKeys.cend(),
        [&settings](const QString& key) { return settings.contains(key); });
    if (!hasLegacy) {
        return;
    }

    // Each fallback is the default the old code read the key with.
    Config m = defaults();
    m.nr4ReductionDb =
        settings.value(kLegacyNr4Reduction, m.nr4ReductionDb).toFloat();
    m.nr4SmoothingPercent =
        settings.value(kLegacyNr4Smoothing, m.nr4SmoothingPercent).toFloat();
    m.nr4WhiteningPercent =
        settings.value(kLegacyNr4Whitening, m.nr4WhiteningPercent).toFloat();
    m.nr4AdaptiveNoise = settingIsTrue(
        settings.value(kLegacyNr4Adaptive, QStringLiteral("True")));
    m.nr4NoiseMethod =
        settings.value(kLegacyNr4Method, m.nr4NoiseMethod).toInt();
    m.nr4MaskingDepth =
        settings.value(kLegacyNr4Masking, m.nr4MaskingDepth).toFloat();
    m.nr4SuppressionStrength =
        settings.value(kLegacyNr4Suppression, m.nr4SuppressionStrength)
            .toFloat();
    m.mnrStrength = settings.value(kLegacyMnrStrength, m.mnrStrength).toFloat();
    m.dfnrAttenLimitDb =
        settings.value(kLegacyDfnrAtten, m.dfnrAttenLimitDb).toFloat();
    m.dfnrPostFilterBeta =
        settings.value(kLegacyDfnrBeta, m.dfnrPostFilterBeta).toFloat();
    m = normalized(m);

    settings.setValue(kRootKey, compactJson(m));
    for (const QString& key : kLegacyKeys) {
        settings.remove(key);
    }
    settings.save();

    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = m;
}

void ClientNrSettingsModel::persist(const Config& config) const
{
    AppSettings& settings = AppSettings::instance();
    settings.setValue(kRootKey, compactJson(config));
    settings.save();
}

void ClientNrSettingsModel::setConfig(const Config& requested)
{
    const Config next = normalized(requested);
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_config == next) {
            return;
        }
        m_config = next;
    }
    persist(next);
    emit configChanged();
}

void ClientNrSettingsModel::setNr4ReductionDb(float db)
{
    Config next = config();
    next.nr4ReductionDb = db;
    setConfig(next);
}

void ClientNrSettingsModel::setNr4SmoothingPercent(float percent)
{
    Config next = config();
    next.nr4SmoothingPercent = percent;
    setConfig(next);
}

void ClientNrSettingsModel::setNr4WhiteningPercent(float percent)
{
    Config next = config();
    next.nr4WhiteningPercent = percent;
    setConfig(next);
}

void ClientNrSettingsModel::setNr4AdaptiveNoise(bool enabled)
{
    Config next = config();
    next.nr4AdaptiveNoise = enabled;
    setConfig(next);
}

void ClientNrSettingsModel::setNr4NoiseMethod(int method)
{
    Config next = config();
    next.nr4NoiseMethod = method;
    setConfig(next);
}

void ClientNrSettingsModel::setNr4MaskingDepth(float value)
{
    Config next = config();
    next.nr4MaskingDepth = value;
    setConfig(next);
}

void ClientNrSettingsModel::setNr4SuppressionStrength(float value)
{
    Config next = config();
    next.nr4SuppressionStrength = value;
    setConfig(next);
}

void ClientNrSettingsModel::setMnrStrength(float value)
{
    Config next = config();
    next.mnrStrength = value;
    setConfig(next);
}

void ClientNrSettingsModel::setDfnrAttenLimitDb(float db)
{
    Config next = config();
    next.dfnrAttenLimitDb = db;
    setConfig(next);
}

void ClientNrSettingsModel::setDfnrPostFilterBeta(float beta)
{
    Config next = config();
    next.dfnrPostFilterBeta = beta;
    setConfig(next);
}

} // namespace AetherSDR
