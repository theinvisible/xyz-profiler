#pragma once

#include <QObject>
#include <QString>

class QSettings;

namespace xyz {

// QSettings-backed user preferences. Settings are stored as INI under
// `QStandardPaths::AppConfigLocation` (e.g.
// `%APPDATA%\xyz-profiler\xyz-profiler.ini` on Windows) so the user can
// easily inspect or hand-edit them.
//
// Every setter persists immediately and emits the matching change signal.
// No "save" / "discard" pattern — the UI is the source of truth.
//
// The library DB path is intentionally NOT in here: it lives under
// AppLocalDataLocation and is fully auto-managed. Adding it would only
// invite footguns (relocating libraries, missing-DB on launch, …).
class SettingsController : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString tmdbApiKey      READ tmdbApiKey      WRITE setTmdbApiKey      NOTIFY tmdbApiKeyChanged)
    Q_PROPERTY(QString imagesDirectory READ imagesDirectory WRITE setImagesDirectory NOTIFY imagesDirectoryChanged)
    // "Dark" | "Light" | "System". Bound directly to Material.theme via
    // a small mapping in QML.
    Q_PROPERTY(QString themeName       READ themeName       WRITE setThemeName       NOTIFY themeNameChanged)

public:
    explicit SettingsController(QObject* parent = nullptr);
    ~SettingsController() override;

    QString tmdbApiKey()      const { return m_tmdbApiKey; }
    QString imagesDirectory() const { return m_imagesDirectory; }
    QString themeName()       const { return m_themeName; }

    // Effective TMDb key: explicit setting wins over the TMDB_API_KEY env
    // var (which is the install-time / CI default). Helper for main.cpp
    // so the precedence rule lives in one place.
    static QString resolveTmdbApiKey(const SettingsController& settings);

    // Convenience for QML form fields that pass an already-stripped
    // `file://`-style URL from a FolderDialog.
    Q_INVOKABLE static QString urlToLocalPath(const QString& url);

public slots:
    void setTmdbApiKey(const QString& key);
    void setImagesDirectory(const QString& dir);
    void setThemeName(const QString& name);

signals:
    void tmdbApiKeyChanged();
    void imagesDirectoryChanged();
    void themeNameChanged();

private:
    void load_();
    void write_(const QString& key, const QVariant& value);

    QSettings* m_store;
    QString    m_tmdbApiKey;
    QString    m_imagesDirectory;
    QString    m_themeName;
};

} // namespace xyz
