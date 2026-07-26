#pragma once

#include <QObject>
#include <QString>

class QSettings;

namespace xyz {

// QSettings-backed user preferences persisted as INI under AppConfigLocation.
// Every setter writes immediately and emits the matching signal.
class SettingsController : public QObject {
    Q_OBJECT

public:
    explicit SettingsController(QObject* parent = nullptr);
    ~SettingsController() override;

    QString tmdbApiKey()           const { return m_tmdbApiKey; }
    QString imagesDirectory()      const { return m_imagesDirectory; }
    QString themeName()            const { return m_themeName; }
    QString viewMode()             const { return m_viewMode; }
    QString visibleTableColumns()  const { return m_visibleTableColumns; }
    QString tableSortRole()        const { return m_tableSortRole; }
    bool    tableSortDescending()  const { return m_tableSortDescending; }
    QString detailSplitterState()  const { return m_detailSplitterState; }
    QString calendarDateBasis()    const { return m_calendarDateBasis; }
    QString calendarView()         const { return m_calendarView; }
    QString collectionStatusFilter() const { return m_collectionStatusFilter; }

    void setTmdbApiKey(const QString& key);
    void setImagesDirectory(const QString& dir);
    void setThemeName(const QString& name);
    void setViewMode(const QString& mode);
    void setVisibleTableColumns(const QString& columns);
    void setTableSortRole(const QString& role);
    void setTableSortDescending(bool desc);
    // Base64-encoded QSplitter::saveState() for the views/detail splitter.
    void setDetailSplitterState(const QString& base64);
    // Calendar window: which date drives it ("release" | "purchase") and the
    // active view ("month" | "year").
    void setCalendarDateBasis(const QString& basis);
    void setCalendarView(const QString& view);
    // Which slice of the collection the views show:
    // "owned" (default) | "wishlist" | "all". See domain/CollectionMembership.h.
    void setCollectionStatusFilter(const QString& status);

    static QString resolveTmdbApiKey(const SettingsController& settings);

signals:
    void tmdbApiKeyChanged();
    void imagesDirectoryChanged();
    void themeNameChanged();
    void viewModeChanged();
    void visibleTableColumnsChanged();
    void tableSortChanged();
    void calendarDateBasisChanged();
    void calendarViewChanged();
    void collectionStatusFilterChanged();

private:
    void load_();
    void write_(const QString& key, const QVariant& value);

    QSettings* m_store;
    QString    m_tmdbApiKey;
    QString    m_imagesDirectory;
    QString    m_themeName;
    QString    m_viewMode;
    QString    m_visibleTableColumns;
    QString    m_tableSortRole;
    bool       m_tableSortDescending = false;
    QString    m_detailSplitterState;
    QString    m_calendarDateBasis;
    QString    m_calendarView;
    QString    m_collectionStatusFilter;
};

} // namespace xyz
