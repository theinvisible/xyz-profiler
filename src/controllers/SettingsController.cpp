#include "SettingsController.h"

#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>
#include <QVariant>

namespace xyz {
namespace {

constexpr auto kKeyTmdb   = "tmdb/api_key";
constexpr auto kKeyImages = "library/images_directory";
constexpr auto kKeyTheme  = "ui/theme";

QString settingsPath()
{
    const QString dir = QStandardPaths::writableLocation(
        QStandardPaths::AppConfigLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/xyz-profiler.ini");
}

} // namespace

SettingsController::SettingsController(QObject* parent)
    : QObject(parent),
      m_store(new QSettings(settingsPath(), QSettings::IniFormat, this)),
      m_themeName(QStringLiteral("Dark"))
{
    load_();
}

SettingsController::~SettingsController() = default;

void SettingsController::load_()
{
    m_tmdbApiKey      = m_store->value(QLatin1String(kKeyTmdb)).toString();
    m_imagesDirectory = m_store->value(QLatin1String(kKeyImages)).toString();
    m_themeName       = m_store->value(QLatin1String(kKeyTheme),
                                       QStringLiteral("Dark")).toString();
}

void SettingsController::write_(const QString& key, const QVariant& value)
{
    m_store->setValue(key, value);
    m_store->sync();
}

void SettingsController::setTmdbApiKey(const QString& key)
{
    const QString trimmed = key.trimmed();
    if (m_tmdbApiKey == trimmed) return;
    m_tmdbApiKey = trimmed;
    write_(QString::fromLatin1(kKeyTmdb), m_tmdbApiKey);
    emit tmdbApiKeyChanged();
}

void SettingsController::setImagesDirectory(const QString& dir)
{
    const QString local = urlToLocalPath(dir);
    if (m_imagesDirectory == local) return;
    m_imagesDirectory = local;
    write_(QString::fromLatin1(kKeyImages), m_imagesDirectory);
    emit imagesDirectoryChanged();
}

void SettingsController::setThemeName(const QString& name)
{
    // Only accept the three known values; silently coerce anything else
    // to "Dark" so a stale .ini doesn't render a black-on-black UI.
    QString next = name;
    if (next != QLatin1String("Dark") &&
        next != QLatin1String("Light") &&
        next != QLatin1String("System")) {
        next = QStringLiteral("Dark");
    }
    if (m_themeName == next) return;
    m_themeName = next;
    write_(QString::fromLatin1(kKeyTheme), m_themeName);
    emit themeNameChanged();
}

QString SettingsController::resolveTmdbApiKey(const SettingsController& settings)
{
    if (!settings.tmdbApiKey().isEmpty()) return settings.tmdbApiKey();
    return qEnvironmentVariable("TMDB_API_KEY");
}

QString SettingsController::urlToLocalPath(const QString& url)
{
    if (url.startsWith(QLatin1String("file:"))) return QUrl(url).toLocalFile();
    return url;
}

} // namespace xyz
