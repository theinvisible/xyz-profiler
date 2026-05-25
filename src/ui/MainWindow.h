#pragma once

#include <QMainWindow>

class QAction;
class QActionGroup;
class QLabel;
class QLineEdit;
class QProgressDialog;
class QSortFilterProxyModel;
class QStackedWidget;
class QTableView;

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
    void buildToolBar_();
    void buildCentralWidget_();
    void buildStatusBar_();
    void connectController_();
    void connectSettings_();

    void onMoviesChanged_();
    void onSelectionChanged_();
    void onImportStateChanged_();
    void onTmdbStateChanged_();
    void switchView_(const QString& mode);
    void showImportDialog_();
    void showSettingsDialog_();
    void setupTableColumnVisibility_();

    LibraryController*   m_controller;
    SettingsController*  m_settings;
    TmdbClient*          m_tmdb;

    // Toolbar
    QLineEdit*   m_searchField  = nullptr;
    QLabel*      m_movieCount   = nullptr;
    QAction*     m_gridAction   = nullptr;
    QAction*     m_listAction   = nullptr;

    // Central
    QStackedWidget*       m_viewStack    = nullptr;
    CoverGridWidget*      m_coverGrid    = nullptr;
    QTableView*           m_tableView    = nullptr;
    QSortFilterProxyModel* m_tableSortProxy = nullptr;
    MovieDetailWidget*    m_detailPane   = nullptr;

    // Progress
    QProgressDialog*      m_progressDlg  = nullptr;
};

} // namespace xyz
