#include "MainWindow.h"

#include "controllers/LibraryController.h"
#include "controllers/SettingsController.h"
#include "models/MovieListModel.h"
#include "models/MovieTreeModel.h"
#include "tmdb/TmdbClient.h"
#include "ui/AddTitleDialog.h"
#include "ui/CoverCache.h"
#include "ui/CoverGridWidget.h"
#include "ui/IconFactory.h"
#include "ui/ImportPreviewDialog.h"
#include "ui/MovieDetailWidget.h"
#include "ui/MovieRowDelegate.h"
#include "ui/SettingsDialog.h"
#include "ui/Theme.h"
#include "ui/TmdbMatchDialog.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QByteArray>
#include <QComboBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QProgressDialog>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QToolBar>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>

namespace xyz {
namespace {

// Map a tree-view column to the cover grid's sort role name (see
// MovieListModel::roleNames()). Columns the grid can't represent fall back to
// title, so both views stay roughly in sync.
QString gridRoleForColumn(int column)
{
    switch (column) {
    case MovieTreeModel::Year:     return QStringLiteral("year");
    case MovieTreeModel::Runtime:  return QStringLiteral("runtime");
    case MovieTreeModel::Format:   return QStringLiteral("format");
    case MovieTreeModel::Rating:   return QStringLiteral("ratingValue");
    case MovieTreeModel::Genres:   return QStringLiteral("genresJoined");
    case MovieTreeModel::Director: return QStringLiteral("directorName");
    default:                       return QStringLiteral("title");
    }
}

} // namespace

MainWindow::MainWindow(LibraryController* controller,
                       SettingsController* settings,
                       TmdbClient* tmdb,
                       QWidget* parent)
    : QMainWindow(parent),
      m_controller(controller),
      m_settings(settings),
      m_tmdb(tmdb)
{
    setWindowTitle(QStringLiteral("XYZ Profiler"));
    resize(1400, 900);

    buildMenuBar_();
    buildToolBar_();
    buildCentralWidget_();
    buildStatusBar_();
    connectController_();
    connectSettings_();

    refreshIcons_();
    switchView_(m_settings->viewMode());
    applySavedSort_();
    onMoviesChanged_();
    onSelectionChanged_();
}

// ---------------------------------------------------------------------------
// Menu bar
// ---------------------------------------------------------------------------
void MainWindow::buildMenuBar_()
{
    auto* mb = menuBar();

    auto* file = mb->addMenu(tr("&File"));
    m_actImport = file->addAction(tr("&Import Collection.xml…"),
                                  this, &MainWindow::showImportDialog_);
    file->addSeparator();
    m_actQuit = file->addAction(tr("&Quit"), qApp, &QApplication::quit);
    m_actQuit->setShortcut(QKeySequence::Quit);

    auto* view = mb->addMenu(tr("&View"));
    auto* viewGroup = new QActionGroup(this);
    m_actViewList = view->addAction(tr("&List View"));
    m_actViewList->setCheckable(true);
    viewGroup->addAction(m_actViewList);
    m_actViewGrid = view->addAction(tr("&Grid View"));
    m_actViewGrid->setCheckable(true);
    viewGroup->addAction(m_actViewGrid);
    connect(m_actViewList, &QAction::triggered, this,
            [this] { m_settings->setViewMode(QStringLiteral("list")); });
    connect(m_actViewGrid, &QAction::triggered, this,
            [this] { m_settings->setViewMode(QStringLiteral("grid")); });
    view->addSeparator();
    m_actTheme = view->addAction(tr("Toggle &Theme"), this, &MainWindow::toggleTheme_);

    auto* coll = mb->addMenu(tr("&Collection"));
    m_actRefresh = coll->addAction(tr("&Refresh"),
                                   m_controller, &LibraryController::refresh);
    m_actRefresh->setShortcut(QKeySequence::Refresh);

    auto* tools = mb->addMenu(tr("&Tools"));
    m_actSettings = tools->addAction(tr("&Settings…"),
                                     this, &MainWindow::showSettingsDialog_);
    m_actSettings->setShortcut(QKeySequence::Preferences);

    auto* help = mb->addMenu(tr("&Help"));
    m_actAbout = help->addAction(tr("&About XYZ Profiler…"),
                                 this, &MainWindow::showAbout_);
}

// ---------------------------------------------------------------------------
// Toolbar
// ---------------------------------------------------------------------------
void MainWindow::buildToolBar_()
{
    auto* tb = addToolBar(tr("Main"));
    tb->setMovable(false);
    tb->setFloatable(false);
    tb->setIconSize(QSize(16, 16));

    const auto spacer = [] {
        auto* w = new QWidget;
        w->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        return w;
    };

    // Primary action: add a new title via TMDb online search.
    m_addBtn = new QToolButton;
    m_addBtn->setObjectName(QStringLiteral("tbPrimary"));
    m_addBtn->setText(tr("Add Title…"));
    m_addBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_addBtn->setToolTip(tr("Search TMDb and add a new title to your collection"));
    connect(m_addBtn, &QToolButton::clicked, this, &MainWindow::showAddTitleDialog_);
    tb->addWidget(m_addBtn);

    tb->addWidget(spacer());

    // Search.
    m_searchField = new QLineEdit;
    m_searchField->setObjectName(QStringLiteral("searchField"));
    m_searchField->setPlaceholderText(tr("Search collection…"));
    m_searchField->setClearButtonEnabled(true);
    m_searchField->setMinimumWidth(220);
    m_searchField->setMaximumWidth(360);
    m_searchIcon = m_searchField->addAction(QIcon(), QLineEdit::LeadingPosition);
    connect(m_searchField, &QLineEdit::returnPressed, this,
            [this] { m_controller->search(m_searchField->text()); });
    connect(m_searchField, &QLineEdit::textChanged, this,
            [this](const QString& t) { if (t.isEmpty()) m_controller->refresh(); });
    tb->addWidget(m_searchField);

    tb->addWidget(spacer());

    // Segmented list/grid toggle.
    auto* seg = new QWidget;
    seg->setObjectName(QStringLiteral("viewSeg"));
    auto* segLay = new QHBoxLayout(seg);
    segLay->setContentsMargins(2, 2, 2, 2);
    segLay->setSpacing(2);
    m_listBtn = new QToolButton;
    m_listBtn->setObjectName(QStringLiteral("segBtn"));
    m_listBtn->setCheckable(true);
    m_listBtn->setToolTip(tr("List view"));
    connect(m_listBtn, &QToolButton::clicked, this,
            [this] { m_settings->setViewMode(QStringLiteral("list")); });
    m_gridBtn = new QToolButton;
    m_gridBtn->setObjectName(QStringLiteral("segBtn"));
    m_gridBtn->setCheckable(true);
    m_gridBtn->setToolTip(tr("Cover view"));
    connect(m_gridBtn, &QToolButton::clicked, this,
            [this] { m_settings->setViewMode(QStringLiteral("grid")); });
    segLay->addWidget(m_listBtn);
    segLay->addWidget(m_gridBtn);
    tb->addWidget(seg);

    // Theme + settings.
    m_themeBtn = new QToolButton;
    m_themeBtn->setObjectName(QStringLiteral("tbIcon"));
    m_themeBtn->setToolTip(tr("Toggle light / dark"));
    connect(m_themeBtn, &QToolButton::clicked, this, &MainWindow::toggleTheme_);
    tb->addWidget(m_themeBtn);

    m_settingsBtn = new QToolButton;
    m_settingsBtn->setObjectName(QStringLiteral("tbIcon"));
    m_settingsBtn->setToolTip(tr("Settings"));
    connect(m_settingsBtn, &QToolButton::clicked, this, &MainWindow::showSettingsDialog_);
    tb->addWidget(m_settingsBtn);
}

// ---------------------------------------------------------------------------
// Central widget
// ---------------------------------------------------------------------------
void MainWindow::buildCentralWidget_()
{
    m_splitter = new QSplitter(Qt::Horizontal);
    m_viewStack = new QStackedWidget;

    // ---- Cover grid (index 0) — filter out box-set children ----------------
    m_gridFilterProxy = new QSortFilterProxyModel(this);
    m_gridFilterProxy->setSourceModel(m_controller->sortProxy());
    m_gridFilterProxy->setFilterRole(MovieListModel::BoxSetParentIdRole);
    m_gridFilterProxy->setFilterRegularExpression(
        QRegularExpression(QStringLiteral("^$")));

    m_coverGrid = new CoverGridWidget;
    m_coverGrid->setModel(m_gridFilterProxy);
    m_viewStack->addWidget(m_coverGrid);

    // ---- Tree view (index 1) — box-set parents expandable ------------------
    m_treeSortProxy = new QSortFilterProxyModel(this);
    m_treeSortProxy->setSourceModel(m_controller->treeModel());
    m_treeSortProxy->setSortRole(Qt::UserRole);
    m_treeSortProxy->setSortLocaleAware(true);
    m_treeSortProxy->setRecursiveFilteringEnabled(true);

    m_treeView = new QTreeView;
    m_treeView->setModel(m_treeSortProxy);
    m_treeView->setItemDelegate(new MovieRowDelegate(m_treeView));
    m_treeView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_treeView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_treeView->setSortingEnabled(true);
    m_treeView->setAlternatingRowColors(false);
    m_treeView->setRootIsDecorated(true);
    m_treeView->setItemsExpandable(true);
    m_treeView->setAnimated(true);
    m_treeView->setUniformRowHeights(true);
    m_treeView->setMouseTracking(true);
    m_treeView->header()->setStretchLastSection(true);
    m_treeView->header()->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(m_treeView->header(), &QWidget::customContextMenuRequested,
            this, [this](const QPoint&) {
        QMenu menu;
        for (int col = 0; col < MovieTreeModel::ColumnCount; ++col) {
            auto* act = menu.addAction(
                m_controller->treeModel()->headerData(col, Qt::Horizontal, Qt::DisplayRole).toString());
            act->setCheckable(true);
            act->setChecked(!m_treeView->isColumnHidden(col));
            connect(act, &QAction::toggled, this, [this, col](bool visible) {
                m_treeView->setColumnHidden(col, !visible);
                QStringList visCols;
                for (int c = 0; c < MovieTreeModel::ColumnCount; ++c) {
                    if (!m_treeView->isColumnHidden(c))
                        visCols << QString::number(c);
                }
                m_settings->setVisibleTableColumns(visCols.join(QStringLiteral(";")));
            });
        }
        menu.exec(QCursor::pos());
    });

    setupTreeColumnVisibility_();

    auto* hdr = m_treeView->header();
    hdr->setStretchLastSection(false);
    hdr->setSectionResizeMode(MovieTreeModel::Title, QHeaderView::Stretch);
    hdr->setSectionResizeMode(MovieTreeModel::Year, QHeaderView::ResizeToContents);
    hdr->setSectionResizeMode(MovieTreeModel::Format, QHeaderView::Fixed);
    hdr->resizeSection(MovieTreeModel::Format, 110);
    hdr->setSectionResizeMode(MovieTreeModel::Rating, QHeaderView::Fixed);
    hdr->resizeSection(MovieTreeModel::Rating, 104);
    hdr->setSectionResizeMode(MovieTreeModel::Genres, QHeaderView::Interactive);
    hdr->resizeSection(MovieTreeModel::Genres, 220);
    hdr->setSectionResizeMode(MovieTreeModel::Director, QHeaderView::Interactive);
    hdr->resizeSection(MovieTreeModel::Director, 160);

    // Persist the sort the user sets via the header and mirror it onto the
    // cover grid, so both views share one (restored-on-startup) sort order.
    connect(m_treeView->header(), &QHeaderView::sortIndicatorChanged, this,
            [this](int column, Qt::SortOrder order) {
        if (m_restoringSort) return;
        m_settings->setTableSortRole(QString::number(column));
        m_settings->setTableSortDescending(order == Qt::DescendingOrder);
        m_controller->sortProxy()->sortByRole(
            gridRoleForColumn(column), order == Qt::DescendingOrder);
    });

    m_viewStack->addWidget(m_treeView);

    m_splitter->addWidget(m_viewStack);

    // ---- Detail pane (right) -----------------------------------------------
    m_detailPane = new MovieDetailWidget;
    m_splitter->addWidget(m_detailPane);

    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 0);
    m_splitter->setSizes({940, 462});
    m_splitter->setChildrenCollapsible(false);
    setCentralWidget(m_splitter);

    // The detail pane is resizable now (no fixed width) — restore the saved
    // splitter position and persist it whenever the user drags the handle.
    const QByteArray splitState = QByteArray::fromBase64(
        m_settings->detailSplitterState().toLatin1());
    if (!splitState.isEmpty())
        m_splitter->restoreState(splitState);
    connect(m_splitter, &QSplitter::splitterMoved, this, [this](int, int) {
        m_settings->setDetailSplitterState(
            QString::fromLatin1(m_splitter->saveState().toBase64()));
    });

    // ---- Signals -----------------------------------------------------------
    connect(m_coverGrid, &CoverGridWidget::movieClicked,
            m_controller, &LibraryController::selectMovie);

    connect(m_treeView, &QAbstractItemView::clicked, this,
            [this](const QModelIndex& idx) {
        const auto srcIdx = m_treeSortProxy->mapToSource(idx);
        const QString id = m_controller->treeModel()->movieIdAtIndex(srcIdx);
        if (!id.isEmpty()) m_controller->selectMovie(id);
    });

    connect(m_detailPane, &MovieDetailWidget::tmdbSearchRequested,
            m_controller, &LibraryController::searchSelectedOnTmdb);
}

void MainWindow::buildStatusBar_()
{
    auto* sb = statusBar();
    sb->setSizeGripEnabled(false);

    m_countLabel = new QLabel;
    sb->addWidget(m_countLabel);

    m_selectionLabel = new QLabel;
    m_selectionLabel->setAlignment(Qt::AlignCenter);
    sb->addWidget(m_selectionLabel, 1);

    m_syncIcon = new QLabel;
    sb->addPermanentWidget(m_syncIcon);
    m_statusLabel = new QLabel(m_controller->statusMessage());
    sb->addPermanentWidget(m_statusLabel);
}

void MainWindow::setupTreeColumnVisibility_()
{
    const QString saved = m_settings->visibleTableColumns();

    static const QSet<int> defaults = {
        MovieTreeModel::Title, MovieTreeModel::Year,
        MovieTreeModel::Genres, MovieTreeModel::Format,
        MovieTreeModel::Rating
    };

    if (saved.isEmpty()) {
        for (int c = 0; c < MovieTreeModel::ColumnCount; ++c)
            m_treeView->setColumnHidden(c, !defaults.contains(c));
        return;
    }

    const QStringList visible = saved.split(QStringLiteral(";"));
    bool numeric = !visible.isEmpty() && !visible.first().isEmpty()
                   && visible.first().at(0).isDigit();

    if (numeric) {
        QSet<int> visCols;
        for (const auto& s : visible) visCols.insert(s.toInt());
        for (int c = 0; c < MovieTreeModel::ColumnCount; ++c)
            m_treeView->setColumnHidden(c, !visCols.contains(c));
    } else {
        for (int c = 0; c < MovieTreeModel::ColumnCount; ++c)
            m_treeView->setColumnHidden(c, !defaults.contains(c));
    }
}

// ---------------------------------------------------------------------------
// Connections
// ---------------------------------------------------------------------------
void MainWindow::connectController_()
{
    connect(m_controller, &LibraryController::moviesChanged,
            this, &MainWindow::onMoviesChanged_);
    connect(m_controller, &LibraryController::selectionChanged,
            this, &MainWindow::onSelectionChanged_);
    connect(m_controller, &LibraryController::statusMessageChanged, this,
            [this] { m_statusLabel->setText(m_controller->statusMessage()); });
    connect(m_controller, &LibraryController::importStateChanged,
            this, &MainWindow::onImportStateChanged_);
    connect(m_controller, &LibraryController::tmdbStateChanged,
            this, &MainWindow::onTmdbStateChanged_);
    connect(m_controller, &LibraryController::coverUpdated, this,
            [this](const QString& path) {
        // The poster file changed in place: bump its cache generation, then
        // repaint the views and re-render the detail pane so the new artwork
        // replaces the cached one.
        CoverCache::bump(path);
        if (m_coverGrid)  m_coverGrid->viewport()->update();
        if (m_treeView)   m_treeView->viewport()->update();
        if (m_detailPane) m_detailPane->refreshTheme();
    });
}

void MainWindow::connectSettings_()
{
    connect(m_settings, &SettingsController::viewModeChanged, this,
            [this] { switchView_(m_settings->viewMode()); });
    connect(m_settings, &SettingsController::themeNameChanged, this, [this] {
        refreshIcons_();
        if (m_treeView)  m_treeView->viewport()->update();
        if (m_coverGrid) m_coverGrid->viewport()->update();
        if (m_detailPane) m_detailPane->refreshTheme();
    });
}

// ---------------------------------------------------------------------------
// Icons (re-tinted on theme change)
// ---------------------------------------------------------------------------
void MainWindow::refreshIcons_()
{
    const Palette& p = Theme::current();
    m_addBtn->setIcon(IconFactory::icon(QStringLiteral("add"), p.accentFg, 17));
    if (m_searchIcon)
        m_searchIcon->setIcon(IconFactory::icon(QStringLiteral("search"), p.text3, 16));
    m_settingsBtn->setIcon(IconFactory::icon(QStringLiteral("settings"), p.text2, 16));
    m_themeBtn->setIcon(IconFactory::icon(
        Theme::isDark() ? QStringLiteral("sun") : QStringLiteral("moon"), p.text2, 16));

    // Segmented buttons depend on which is active.
    const bool isList = (m_settings->viewMode() == QLatin1String("list"));
    m_listBtn->setIcon(IconFactory::icon(QStringLiteral("list"),
                                         isList ? p.accentFg : p.text2, 16));
    m_gridBtn->setIcon(IconFactory::icon(QStringLiteral("grid"),
                                         !isList ? p.accentFg : p.text2, 16));

    if (m_syncIcon)
        m_syncIcon->setPixmap(IconFactory::pixmap(QStringLiteral("refresh"), p.text3, 13,
                                                  1.6, devicePixelRatioF()));
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------
void MainWindow::onMoviesChanged_()
{
    m_countLabel->setText(tr("%1 movies").arg(m_controller->movieCount()));
    m_searchField->setEnabled(m_controller->movieCount() > 0);
}

void MainWindow::onSelectionChanged_()
{
    if (m_controller->hasSelection()) {
        const Movie& m = m_controller->selectedMovie();
        m_detailPane->updateFromMovie(m);
        QStringList parts;
        parts << m.title;
        if (!m.format.isEmpty())   parts << m.format;
        if (!m.locationId.isEmpty()) parts << m.locationId;
        m_selectionLabel->setText(parts.join(QStringLiteral("  ·  ")));
    } else {
        m_detailPane->clearSelection();
        m_selectionLabel->setText(tr("No movie selected"));
    }
}

void MainWindow::onImportStateChanged_()
{
    if (m_controller->previewActive()) {
        ImportPreviewDialog dlg(this);
        dlg.setPreview(m_controller->previewMovieCount(),
                       m_controller->previewSourceName(),
                       m_controller->previewSampleTitles(),
                       m_controller->previewImagesDir());
        if (dlg.exec() == QDialog::Accepted)
            m_controller->commitImport();
        else
            m_controller->cancelImport();
        return;
    }

    if (m_controller->importInProgress()) {
        if (!m_progressDlg) {
            m_progressDlg = new QProgressDialog(this);
            m_progressDlg->setWindowTitle(tr("Importing…"));
            m_progressDlg->setWindowModality(Qt::WindowModal);
            m_progressDlg->setCancelButton(nullptr);
            m_progressDlg->setMinimumDuration(0);
        }
        m_progressDlg->setLabelText(m_controller->importStage());
        m_progressDlg->setMaximum(std::max(1, m_controller->importTotal()));
        m_progressDlg->setValue(m_controller->importCurrent());
        m_progressDlg->show();
    } else {
        if (m_progressDlg) {
            m_progressDlg->close();
            delete m_progressDlg;
            m_progressDlg = nullptr;
        }
    }
}

void MainWindow::onTmdbStateChanged_()
{
    if (m_controller->tmdbSearching())
        return;

    const auto& candidates = m_controller->tmdbCandidates();
    if (candidates.isEmpty() && m_controller->tmdbSearchError().isEmpty())
        return;

    TmdbMatchDialog dlg(this);
    if (!m_controller->tmdbSearchError().isEmpty())
        dlg.setError(m_controller->tmdbSearchError());

    if (!candidates.isEmpty() && m_tmdb) {
        dlg.setCandidates(candidates,
            [this](const QString& path, const QString& size) {
                return m_tmdb->imageUrl(path, size);
            });
        dlg.loadPosters(m_tmdb->network());
    }

    if (dlg.exec() == QDialog::Accepted && dlg.selectedTmdbId() > 0)
        m_controller->pickTmdbMatch(dlg.selectedTmdbId());
    else
        m_controller->clearTmdbCandidates();
}

void MainWindow::switchView_(const QString& mode)
{
    const bool isList = (mode == QLatin1String("list"));
    m_viewStack->setCurrentIndex(isList ? 1 : 0);
    m_listBtn->setChecked(isList);
    m_gridBtn->setChecked(!isList);
    if (m_actViewList) m_actViewList->setChecked(isList);
    if (m_actViewGrid) m_actViewGrid->setChecked(!isList);

    const Palette& p = Theme::current();
    m_listBtn->setIcon(IconFactory::icon(QStringLiteral("list"),
                                         isList ? p.accentFg : p.text2, 16));
    m_gridBtn->setIcon(IconFactory::icon(QStringLiteral("grid"),
                                         !isList ? p.accentFg : p.text2, 16));
}

void MainWindow::applySavedSort_()
{
    bool ok = false;
    int col = m_settings->tableSortRole().toInt(&ok);
    if (!ok || col < 0 || col >= MovieTreeModel::ColumnCount)
        col = MovieTreeModel::Title;
    const bool desc = m_settings->tableSortDescending();
    const Qt::SortOrder order = desc ? Qt::DescendingOrder : Qt::AscendingOrder;

    // Restore without re-persisting (the header signal would fire otherwise).
    m_restoringSort = true;
    m_treeView->sortByColumn(col, order);
    m_treeView->header()->setSortIndicator(col, order);
    m_restoringSort = false;

    m_controller->sortProxy()->sortByRole(gridRoleForColumn(col), desc);
}

void MainWindow::toggleTheme_()
{
    m_settings->setThemeName(m_settings->themeName() == QLatin1String("Dark")
                                 ? QStringLiteral("Light")
                                 : QStringLiteral("Dark"));
}

void MainWindow::showImportDialog_()
{
    const QString file = QFileDialog::getOpenFileName(
        this, tr("Import DVD Profiler Collection.xml"),
        {}, tr("DVD Profiler XML (Collection.xml *.xml)"));
    if (file.isEmpty()) return;
    m_controller->beginImport(file, m_settings->imagesDirectory());
}

void MainWindow::showAddTitleDialog_()
{
    if (!m_controller->libraryOpen()) {
        QMessageBox::information(this, tr("Add a Title"),
            tr("Open a library before adding titles."));
        return;
    }
    if (!m_tmdb || !m_tmdb->hasApiKey()) {
        QMessageBox::information(this, tr("Add a Title"),
            tr("TMDb is not configured. Set your TMDb API key in Settings first."));
        return;
    }
    AddTitleDialog dlg(m_tmdb, this);
    if (dlg.exec() == QDialog::Accepted && dlg.selectedTmdbId() > 0)
        m_controller->addMovieFromTmdb(dlg.selectedTmdbId(), dlg.selectedPosterPath());
}

void MainWindow::showSettingsDialog_()
{
    SettingsDialog dlg(m_settings, this);
    dlg.exec();
}

void MainWindow::showAbout_()
{
    QMessageBox::about(this, tr("About XYZ Profiler"),
        tr("<h3>XYZ Profiler</h3>"
           "<p>A modern desktop manager for your DVD / Blu-ray / UHD "
           "collection — a slimmed-down successor to DVD Profiler 4.</p>"
           "<p>Built with Qt 6 Widgets.</p>"));
}

} // namespace xyz
