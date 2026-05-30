#pragma once

#include <QMainWindow>
#include <QStringList>

class QAction;
class QComboBox;
class QLabel;
class QLineEdit;
class QProgressDialog;
class QSortFilterProxyModel;
class QSplitter;
class QStackedWidget;
class QTimer;
class QToolButton;
class QTreeView;

namespace xyz {

class CoverGridWidget;
class LibraryController;
class MovieDetailWidget;
class SettingsController;
class TmdbClient;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(LibraryController* controller,
               SettingsController* settings,
               TmdbClient* tmdb,
               QWidget* parent = nullptr);

private:
    void buildMenuBar_();
    void buildToolBar_();
    void buildCentralWidget_();
    void buildStatusBar_();
    void connectController_();
    void connectSettings_();

    void refreshIcons_();
    void onMoviesChanged_();
    void onSelectionChanged_();
    void onImportStateChanged_();
    void onTmdbStateChanged_();
    void switchView_(const QString& mode);
    void applySavedSort_();
    void toggleTheme_();
    void showImportDialog_();
    void showAddTitleDialog_();
    void showEditDialog_(const QString& movieId);
    void confirmDeleteMovie_(const QString& movieId);
    void showMovieContextMenu_(const QString& movieId, const QPoint& globalPos);
    void showBulkMatchDialog_(const QStringList& movieIds);
    void onBulkMatchStateChanged_();
    QStringList selectedMovieIds_() const;   // ids selected in the active view
    QStringList bulkMatchTargetIds_(const QStringList& movieIds) const;
    void updateBulkActionEnabled_();         // enable Match button per selection
    void showSettingsDialog_();
    void showAbout_();
    void setupTreeColumnVisibility_();

    LibraryController*   m_controller;
    SettingsController*  m_settings;
    TmdbClient*          m_tmdb;

    // Menu actions
    QAction* m_actImport   = nullptr;
    QAction* m_actQuit     = nullptr;
    QAction* m_actViewList = nullptr;
    QAction* m_actViewGrid = nullptr;
    QAction* m_actTheme    = nullptr;
    QAction* m_actRefresh  = nullptr;
    QAction* m_actSettings = nullptr;
    QAction* m_actAbout    = nullptr;

    // Toolbar
    QToolButton* m_addBtn      = nullptr;
    QToolButton* m_editBtn     = nullptr;
    QToolButton* m_deleteBtn   = nullptr;
    QToolButton* m_matchBtn    = nullptr;
    QLineEdit*   m_searchField = nullptr;
    QAction*     m_searchIcon  = nullptr;
    QToolButton* m_listBtn     = nullptr;
    QToolButton* m_gridBtn     = nullptr;
    QToolButton* m_themeBtn    = nullptr;
    QToolButton* m_settingsBtn = nullptr;

    // Status bar
    QLabel* m_countLabel     = nullptr;
    QLabel* m_selectionLabel = nullptr;
    QLabel* m_syncIcon       = nullptr;
    QLabel* m_statusLabel    = nullptr;

    // Central
    QSplitter*             m_splitter        = nullptr;
    QStackedWidget*        m_viewStack       = nullptr;
    CoverGridWidget*       m_coverGrid       = nullptr;
    QSortFilterProxyModel* m_gridFilterProxy = nullptr;
    QTreeView*             m_treeView        = nullptr;
    QSortFilterProxyModel* m_treeSortProxy   = nullptr;
    MovieDetailWidget*     m_detailPane      = nullptr;
    bool                   m_restoringSort   = false;

    // Coalesces rapid selection changes (held arrow key) so the heavy detail
    // pane rebuild runs once when navigation settles, not once per row.
    QTimer*                m_detailUpdateTimer = nullptr;

    // Progress
    QProgressDialog*       m_progressDlg     = nullptr;
    QProgressDialog*       m_bulkProgressDlg = nullptr;
};

} // namespace xyz
