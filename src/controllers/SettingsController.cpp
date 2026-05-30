#include "SettingsController.h"

#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <QVariant>

namespace xyz {
namespace {

constexpr auto kKeyTmdb         = "tmdb/api_key";
constexpr auto kKeyImages       = "library/images_directory";
constexpr auto kKeyTheme        = "ui/theme";
constexpr auto kKeyViewMode     = "ui/view_mode";
constexpr auto kKeyTableCols    = "ui/table_columns";
constexpr auto kKeyTableSortRole = "ui/table_sort_role";
constexpr auto kKeyTableSortDesc = "ui/table_sort_desc";
constexpr auto kKeySplitterState = "ui/detail_splitter_state";

constexpr auto kDefaultColumns = "title;year;runtime;format;ratingValue;directorName";

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
      m_themeName(QStringLiteral("Dark")),
      m_viewMode(QStringLiteral("grid")),
      m_visibleTableColumns(QString::fromLatin1(kDefaultColumns))
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
    m_viewMode        = m_store->value(QLatin1String(kKeyViewMode),
                                       QStringLiteral("grid")).toString();
    m_visibleTableColumns = m_store->value(QLatin1String(kKeyTableCols),
                                           QString::fromLatin1(kDefaultColumns)).toString();
    m_tableSortRole   = m_store->value(QLatin1String(kKeyTableSortRole)).toString();
    m_tableSortDescending = m_store->value(QLatin1String(kKeyTableSortDesc), false).toBool();
    m_detailSplitterState = m_store->value(QLatin1String(kKeySplitterState)).toString();
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
    if (m_imagesDirectory == dir) return;
    m_imagesDirectory = dir;
    write_(QString::fromLatin1(kKeyImages), m_imagesDirectory);
    emit imagesDirectoryChanged();
}

void SettingsController::setThemeName(const QString& name)
{
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

void SettingsController::setViewMode(const QString& mode)
{
    const QString next = (mode == QLatin1String("list")) ? QStringLiteral("list")
                                                         : QStringLiteral("grid");
    if (m_viewMode == next) return;
    m_viewMode = next;
    write_(QString::fromLatin1(kKeyViewMode), m_viewMode);
    emit viewModeChanged();
}

void SettingsController::setVisibleTableColumns(const QString& columns)
{
    if (m_visibleTableColumns == columns) return;
    m_visibleTableColumns = columns;
    write_(QString::fromLatin1(kKeyTableCols), m_visibleTableColumns);
    emit visibleTableColumnsChanged();
}

void SettingsController::setTableSortRole(const QString& role)
{
    if (m_tableSortRole == role) return;
    m_tableSortRole = role;
    write_(QString::fromLatin1(kKeyTableSortRole), m_tableSortRole);
    emit tableSortChanged();
}

void SettingsController::setTableSortDescending(bool desc)
{
    if (m_tableSortDescending == desc) return;
    m_tableSortDescending = desc;
    write_(QString::fromLatin1(kKeyTableSortDesc), m_tableSortDescending);
    emit tableSortChanged();
}

void SettingsController::setDetailSplitterState(const QString& base64)
{
    // Persisted-only (no live listeners), so this intentionally emits no signal.
    if (m_detailSplitterState == base64) return;
    m_detailSplitterState = base64;
    write_(QString::fromLatin1(kKeySplitterState), m_detailSplitterState);
}

QString SettingsController::resolveTmdbApiKey(const SettingsController& settings)
{
    if (!settings.tmdbApiKey().isEmpty()) return settings.tmdbApiKey();
    return qEnvironmentVariable("TMDB_API_KEY");
}

} // namespace xyz
