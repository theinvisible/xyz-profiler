#include "MainWindow.h"

#include "controllers/LibraryController.h"
#include "controllers/SettingsController.h"
#include "models/MovieListModel.h"
#include "models/MovieTreeModel.h"
#include "tmdb/TmdbClient.h"
#include "ui/BulkTmdbMatchDialog.h"
#include "ui/CalendarWindow.h"
#include "ui/CoverCache.h"
#include "ui/CoverGridWidget.h"
#include "ui/CoverLoader.h"
#include "ui/EditMovieDialog.h"
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
#include <QItemSelectionModel>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QProgressDialog>
#include <QDateTime>
#include <QRegularExpression>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTreeView>
#include <QUuid>
#include <QVBoxLayout>

namespace xyz {
namespace {

// Map a tree-view column to the cover grid's sort role name (see
// MovieListModel::roleNames()). The role keys are chosen to match exactly what
// MovieTreeModel::columnData returns for Qt::UserRole, so a column sorted in
// the list view produces the same order in the grid. Columns the grid can't
// represent fall back to the sort title.
QString gridRoleForColumn(int column)
{
    switch (column) {
    case MovieTreeModel::Title:         return QStringLiteral("sortTitle");
    case MovieTreeModel::OriginalTitle: return QStringLiteral("originalTitle");
    case MovieTreeModel::SortTitle:     return QStringLiteral("sortTitle");
    case MovieTreeModel::Year:          return QStringLiteral("year");
    case MovieTreeModel::Runtime:       return QStringLiteral("runtime");
    case MovieTreeModel::Format:        return QStringLiteral("format");
    case MovieTreeModel::Rating:        return QStringLiteral("reviewFilm");
    case MovieTreeModel::RatingAge:     return QStringLiteral("ratingAge");
    case MovieTreeModel::Director:      return QStringLiteral("directorName");
    case MovieTreeModel::Genres:        return QStringLiteral("genresJoined");
    case MovieTreeModel::Studios:       return QStringLiteral("studiosJoined");
    case MovieTreeModel::CaseType:      return QStringLiteral("caseType");
    case MovieTreeModel::AspectRatio:   return QStringLiteral("aspectRatio");
    case MovieTreeModel::TmdbId:        return QStringLiteral("tmdbId");
    case MovieTreeModel::PurchaseDate:  return QStringLiteral("purchaseDate");
    case MovieTreeModel::Loaned:        return QStringLiteral("isLoaned");
    case MovieTreeModel::BoxSetParent:  return QStringLiteral("isBoxSetParent");
    default:                            return QStringLiteral("sortTitle");
    }
}

// Assign a standard-key shortcut to an action, but ONLY when it resolves to a
// real accelerator. On Windows, QKeySequence::Quit and ::Preferences resolve to
// the named system keys Qt::Key_Exit / Qt::Key_Settings, whose text is the bare
// word "Exit" / "Settings". A QMenu paints that word in the item's shortcut
// column, and with the German Qt translation loaded it becomes "Beenden" /
// "Einstellungen" — so the entry looks doubled ("Einstellungen … Einstellungen").
// We only keep the shortcut when it carries a modifier or is a function key
// (e.g. Linux's Ctrl+Q, or F5 for Refresh); the bare named-key sequences are
// dropped so nothing phantom shows in the menu.
void setStandardShortcut(QAction* action, QKeySequence::StandardKey key)
{
    const QString portable = QKeySequence(key).toString(QKeySequence::PortableText);
    static const QRegularExpression fnKey(QStringLiteral("^F\\d+$"));
    if (portable.contains(QLatin1Char('+')) || fnKey.match(portable).hasMatch())
        action->setShortcut(key);
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
    setStandardShortcut(m_actQuit, QKeySequence::Quit);

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
    setStandardShortcut(m_actRefresh, QKeySequence::Refresh);

    auto* tools = mb->addMenu(tr("&Tools"));
    m_actSettings = tools->addAction(tr("&Settings…"),
                                     this, &MainWindow::showSettingsDialog_);
    setStandardShortcut(m_actSettings, QKeySequence::Preferences);

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

    // Edit / delete the selected title (disabled until something is selected).
    m_editBtn = new QToolButton;
    m_editBtn->setObjectName(QStringLiteral("tbIcon"));
    m_editBtn->setToolTip(tr("Edit the selected title"));
    m_editBtn->setEnabled(false);
    connect(m_editBtn, &QToolButton::clicked, this,
            [this] { showEditDialog_(m_controller->selectedId()); });
    tb->addWidget(m_editBtn);

    m_deleteBtn = new QToolButton;
    m_deleteBtn->setObjectName(QStringLiteral("tbIcon"));
    m_deleteBtn->setToolTip(tr("Delete the selected title"));
    m_deleteBtn->setEnabled(false);
    connect(m_deleteBtn, &QToolButton::clicked, this,
            [this] { confirmDeleteMovie_(m_controller->selectedId()); });
    tb->addWidget(m_deleteBtn);

    // Bulk-match the selected titles against TMDb.
    m_matchBtn = new QToolButton;
    m_matchBtn->setObjectName(QStringLiteral("tbIcon"));
    m_matchBtn->setToolTip(tr("Match the selected titles on TMDb"));
    m_matchBtn->setEnabled(false);
    connect(m_matchBtn, &QToolButton::clicked, this,
            [this] { showBulkMatchDialog_(selectedMovieIds_()); });
    tb->addWidget(m_matchBtn);

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

    // Calendar (stats-over-time window).
    m_calendarBtn = new QToolButton;
    m_calendarBtn->setObjectName(QStringLiteral("tbIcon"));
    m_calendarBtn->setToolTip(tr("Calendar — see your films over time"));
    connect(m_calendarBtn, &QToolButton::clicked, this, &MainWindow::showCalendarWindow_);
    tb->addWidget(m_calendarBtn);

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

    // ---- Cover grid (index 0) — hide box-set parent titles -----------------
    m_gridFilterProxy = new QSortFilterProxyModel(this);
    m_gridFilterProxy->setSourceModel(m_controller->sortProxy());
    m_gridFilterProxy->setFilterRole(MovieListModel::IsBoxSetParentRole);
    m_gridFilterProxy->setFilterRegularExpression(
        QRegularExpression(QStringLiteral("^false$")));

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
    // Extended (Ctrl/Shift) multi-selection enables bulk actions; single click
    // and arrow keys still drive the detail pane via currentChanged.
    m_treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_treeView->setSortingEnabled(true);
    m_treeView->setAlternatingRowColors(false);
    m_treeView->setRootIsDecorated(true);
    m_treeView->setItemsExpandable(true);
    m_treeView->setAnimated(true);
    m_treeView->setUniformRowHeights(true);
    m_treeView->setMouseTracking(true);
    // Smooth pixel scrolling — the per-item default jumps a full 38px row
    // per step, which reads as stutter when flinging through hundreds of
    // titles.
    m_treeView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
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
        // The cover grid sits behind a second (filter-only) proxy; force it to
        // re-map so it reflects the inner proxy's new order immediately.
        m_gridFilterProxy->invalidate();
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
    // Debounced: splitterMoved fires per mouse pixel, and persisting goes
    // through QSettings::sync() — a synchronous INI rewrite on disk. Doing
    // that per drag event is what made dragging the handle feel sluggish;
    // save once, shortly after the drag settles.
    auto* splitterSave = new QTimer(this);
    splitterSave->setSingleShot(true);
    splitterSave->setInterval(300);
    const auto persistSplitter = [this] {
        m_settings->setDetailSplitterState(
            QString::fromLatin1(m_splitter->saveState().toBase64()));
    };
    connect(splitterSave, &QTimer::timeout, this, persistSplitter);
    connect(m_splitter, &QSplitter::splitterMoved, splitterSave,
            qOverload<>(&QTimer::start));
    // A drag followed by an immediate quit must not lose the new position:
    // flush a pending save before shutdown.
    connect(qApp, &QCoreApplication::aboutToQuit, this,
            [splitterSave, persistSplitter] {
        if (splitterSave->isActive()) {
            splitterSave->stop();
            persistSplitter();
        }
    });

    // ---- Signals -----------------------------------------------------------
    connect(m_coverGrid, &CoverGridWidget::movieClicked,
            m_controller, &LibraryController::selectMovie);

    // currentChanged (not clicked) so arrow-key navigation also drives the
    // detail pane, not just mouse clicks.
    connect(m_treeView->selectionModel(), &QItemSelectionModel::currentChanged,
            this, [this](const QModelIndex& cur, const QModelIndex&) {
        if (!cur.isValid()) return;
        const auto srcIdx = m_treeSortProxy->mapToSource(cur);
        const QString id = m_controller->treeModel()->movieIdAtIndex(srcIdx);
        if (!id.isEmpty()) m_controller->selectMovie(id);
    });

    connect(m_detailPane, &MovieDetailWidget::tmdbSearchRequested,
            m_controller, &LibraryController::searchSelectedOnTmdb);

    // Right-click context menu (Edit / Delete) on both views.
    m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_treeView, &QWidget::customContextMenuRequested, this,
            [this](const QPoint& pos) {
        const QModelIndex idx = m_treeView->indexAt(pos);
        if (!idx.isValid()) return;
        const auto srcIdx = m_treeSortProxy->mapToSource(idx);
        const QString id = m_controller->treeModel()->movieIdAtIndex(srcIdx);
        showMovieContextMenu_(id, m_treeView->viewport()->mapToGlobal(pos));
    });

    m_coverGrid->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_coverGrid, &QWidget::customContextMenuRequested, this,
            [this](const QPoint& pos) {
        const QModelIndex idx = m_coverGrid->indexAt(pos);
        if (!idx.isValid()) return;
        const QString id = idx.data(MovieListModel::IdRole).toString();
        showMovieContextMenu_(id, m_coverGrid->viewport()->mapToGlobal(pos));
    });

    // Double-click a title in either view to open the edit dialog. Box-set
    // parents still expand via the decoration arrow (expand-on-double-click is
    // off), so a double-click means "edit" uniformly instead of also toggling
    // the row.
    m_treeView->setExpandsOnDoubleClick(false);
    connect(m_treeView, &QAbstractItemView::doubleClicked, this,
            [this](const QModelIndex& idx) {
        const auto srcIdx = m_treeSortProxy->mapToSource(idx);
        showEditDialog_(m_controller->treeModel()->movieIdAtIndex(srcIdx));
    });

    connect(m_coverGrid, &QAbstractItemView::doubleClicked, this,
            [this](const QModelIndex& idx) {
        showEditDialog_(idx.data(MovieListModel::IdRole).toString());
    });

    // Keep the bulk-match button's enabled state in sync with the multi-selection
    // in whichever view is active.
    connect(m_treeView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, [this](const QItemSelection&, const QItemSelection&) {
        updateBulkActionEnabled_();
    });
    connect(m_coverGrid->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, [this](const QItemSelection&, const QItemSelection&) {
        updateBulkActionEnabled_();
    });
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
    // Debounce timer for the detail pane: a selection change (re)starts it, and
    // only the trailing tick rebuilds the pane — so holding an arrow key skips
    // the intermediate rows. ~70 ms is below the key-repeat interval, so a held
    // key keeps resetting it and the pane updates the moment navigation pauses.
    m_detailUpdateTimer = new QTimer(this);
    m_detailUpdateTimer->setSingleShot(true);
    m_detailUpdateTimer->setInterval(70);
    connect(m_detailUpdateTimer, &QTimer::timeout, this, [this] {
        if (m_controller->hasSelection())
            m_detailPane->updateFromMovie(m_controller->selectedMovie());
    });

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
    connect(m_controller, &LibraryController::bulkMatchStateChanged,
            this, &MainWindow::onBulkMatchStateChanged_);
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
    // Row-swatch decodes finish on the worker pool; repaint the list so the
    // placeholder gets replaced. (The cover grid wires itself up in its own
    // constructor.)
    connect(CoverLoader::instance(), &CoverLoader::coverReady, this,
            [this] { if (m_treeView) m_treeView->viewport()->update(); });
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
        if (m_calendarWindow) m_calendarWindow->refreshTheme();
    });
}

// ---------------------------------------------------------------------------
// Icons (re-tinted on theme change)
// ---------------------------------------------------------------------------
void MainWindow::refreshIcons_()
{
    const Palette& p = Theme::current();
    m_addBtn->setIcon(IconFactory::icon(QStringLiteral("add"), p.accentFg, 17));
    if (m_editBtn)
        m_editBtn->setIcon(IconFactory::icon(QStringLiteral("edit"), p.text2, 16));
    if (m_deleteBtn)
        m_deleteBtn->setIcon(IconFactory::icon(QStringLiteral("trash"), p.text2, 16));
    if (m_matchBtn)
        m_matchBtn->setIcon(IconFactory::icon(QStringLiteral("refresh"), p.text2, 16));
    if (m_searchIcon)
        m_searchIcon->setIcon(IconFactory::icon(QStringLiteral("search"), p.text3, 16));
    if (m_calendarBtn)
        m_calendarBtn->setIcon(IconFactory::icon(QStringLiteral("calendar"), p.text2, 16));
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
    const bool hasSel = m_controller->hasSelection();
    if (m_editBtn)   m_editBtn->setEnabled(hasSel);
    if (m_deleteBtn) m_deleteBtn->setEnabled(hasSel);

    if (m_controller->hasSelection()) {
        const Movie& m = m_controller->selectedMovie();
        // Status-bar text is cheap — update it now for instant feedback. The
        // heavy detail-pane rebuild is deferred via the debounce timer so rapid
        // cursor navigation doesn't rebuild the pane for every passed-over row.
        QStringList parts;
        parts << m.title;
        if (!m.format.isEmpty())   parts << m.format;
        if (!m.locationId.isEmpty()) parts << m.locationId;
        m_selectionLabel->setText(parts.join(QStringLiteral("  ·  ")));
        m_detailUpdateTimer->start();
    } else {
        m_detailUpdateTimer->stop();
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
        // Local pointer + setValue() LAST: a modal QProgressDialog::setValue()
        // spins processEvents(), which can deliver the worker's finished()
        // re-entrantly and null m_progressDlg mid-call.
        QProgressDialog* dlg = m_progressDlg;
        dlg->setLabelText(m_controller->importStage());
        dlg->setMaximum(std::max(1, m_controller->importTotal()));
        dlg->show();
        dlg->setValue(m_controller->importCurrent());
    } else {
        if (m_progressDlg) {
            // deleteLater, not delete: a modal QProgressDialog::setValue() spins
            // processEvents(), so this branch can run with a setValue() on this
            // dialog still on the stack — deleting it outright is a use-after-free.
            m_progressDlg->close();
            m_progressDlg->deleteLater();
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
    m_gridFilterProxy->invalidate();
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

    Movie movie;
    movie.id = QStringLiteral("manual:%1").arg(
        QUuid::createUuid().toString(QUuid::WithoutBraces));
    movie.format = QStringLiteral("DVD");
    movie.membership.type = QStringLiteral("Owned");
    movie.membership.isPartOfOwnedCollection = true;
    movie.profileTimestamp = QDateTime::currentDateTime();
    movie.lastEdited = movie.profileTimestamp;

    EditMovieDialog dlg(movie, m_tmdb, true, this);
    if (dlg.exec() != QDialog::Accepted) return;

    const Movie edited = dlg.editedMovie();
    m_controller->updateMovie(edited);
    m_controller->selectMovie(edited.id);
    m_controller->importCoverImagesForMovie(edited.id,
                                            dlg.selectedFrontCoverPath(),
                                            dlg.selectedBackCoverPath());
    if (dlg.selectedFrontCoverPath().isEmpty() && !dlg.tmdbPosterPath().isEmpty())
        m_controller->downloadTmdbPosterForMovie(edited.id, dlg.tmdbPosterPath());
}

void MainWindow::showEditDialog_(const QString& movieId)
{
    if (movieId.isEmpty()) return;
    // Edit a full copy so untouched fields (cast, discs, box set, tmdbId) survive.
    if (m_controller->selectedId() != movieId)
        m_controller->selectMovie(movieId);
    if (!m_controller->hasSelection()) return;

    EditMovieDialog dlg(m_controller->selectedMovie(), m_tmdb, false, this);
    if (dlg.exec() == QDialog::Accepted) {
        const Movie edited = dlg.editedMovie();
        m_controller->updateMovie(edited);
        m_controller->importCoverImagesForMovie(edited.id,
                                                dlg.selectedFrontCoverPath(),
                                                dlg.selectedBackCoverPath());
        if (dlg.selectedFrontCoverPath().isEmpty() && !dlg.tmdbPosterPath().isEmpty())
            m_controller->downloadTmdbPosterForMovie(edited.id, dlg.tmdbPosterPath());
    }
}

void MainWindow::confirmDeleteMovie_(const QString& movieId)
{
    if (movieId.isEmpty()) return;

    // Resolve the title for the confirmation text.
    QString title = movieId;
    if (const Movie* m = m_controller->listModel()->find(movieId))
        title = m->title;
    else if (m_controller->selectedId() == movieId)
        title = m_controller->selectedMovie().title;

    const auto choice = QMessageBox::warning(
        this, tr("Delete Title"),
        tr("Delete \"%1\" from your collection?\nThis cannot be undone.").arg(title),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (choice == QMessageBox::Yes)
        m_controller->deleteMovie(movieId);
}

void MainWindow::showMovieContextMenu_(const QString& movieId, const QPoint& globalPos)
{
    if (movieId.isEmpty()) return;
    // Target the right-clicked row for both the menu actions and the views.
    if (m_controller->selectedId() != movieId)
        m_controller->selectMovie(movieId);

    const Palette& p = Theme::current();
    QMenu menu(this);
    QAction* editAct = menu.addAction(
        IconFactory::icon(QStringLiteral("edit"), p.text2, 16), tr("Edit…"));
    QAction* delAct = menu.addAction(
        IconFactory::icon(QStringLiteral("trash"), p.text2, 16), tr("Delete…"));

    // "Match on TMDb…" operates on the whole multi-selection. If the
    // right-clicked row isn't part of the current selection, fall back to it.
    QStringList bulkIds = selectedMovieIds_();
    if (!bulkIds.contains(movieId)) bulkIds = { movieId };
    const QStringList targetIds = bulkMatchTargetIds_(bulkIds);
    menu.addSeparator();
    QAction* matchAct = menu.addAction(
        IconFactory::icon(QStringLiteral("refresh"), p.text2, 16),
        targetIds.size() > 1 ? tr("Match %n title(s) on TMDb…", "", targetIds.size())
                             : tr("Match on TMDb…"));

    QAction* chosen = menu.exec(globalPos);
    if (chosen == editAct)       showEditDialog_(movieId);
    else if (chosen == delAct)   confirmDeleteMovie_(movieId);
    else if (chosen == matchAct) showBulkMatchDialog_(bulkIds);
}

QStringList MainWindow::selectedMovieIds_() const
{
    QStringList ids;
    const bool isList = (m_settings->viewMode() == QLatin1String("list"));
    if (isList) {
        const auto rows = m_treeView->selectionModel()->selectedRows();
        for (const QModelIndex& idx : rows) {
            const auto srcIdx = m_treeSortProxy->mapToSource(idx);
            const QString id = m_controller->treeModel()->movieIdAtIndex(srcIdx);
            if (!id.isEmpty()) ids << id;
        }
    } else {
        const auto rows = m_coverGrid->selectionModel()->selectedRows();
        // Grid is a flat list (selectedRows works on column 0); fall back to
        // selectedIndexes if the model has no row concept.
        const auto idxs = rows.isEmpty()
            ? m_coverGrid->selectionModel()->selectedIndexes() : rows;
        for (const QModelIndex& idx : idxs) {
            const QString id = idx.data(MovieListModel::IdRole).toString();
            if (!id.isEmpty() && !ids.contains(id)) ids << id;
        }
    }
    return ids;
}

QStringList MainWindow::bulkMatchTargetIds_(const QStringList& movieIds) const
{
    QStringList out;
    QSet<QString> seen;

    const auto appendId = [&out, &seen](const QString& id) {
        if (id.isEmpty() || seen.contains(id)) return;
        seen.insert(id);
        out << id;
    };

    const auto* model = m_controller ? m_controller->listModel() : nullptr;
    if (!model) {
        for (const QString& id : movieIds)
            appendId(id);
        return out;
    }

    for (const QString& id : movieIds) {
        const Movie* movie = model->find(id);
        if (!movie) {
            appendId(id);
            continue;
        }

        if (!movie->boxSet.isParent) {
            appendId(id);
            continue;
        }

        for (const QString& childId : movie->boxSet.childIds) {
            if (model->find(childId))
                appendId(childId);
        }

        for (const Movie& candidate : model->movies()) {
            if (candidate.boxSet.parentId == movie->id)
                appendId(candidate.id);
        }
    }

    return out;
}

void MainWindow::updateBulkActionEnabled_()
{
    if (m_matchBtn)
        m_matchBtn->setEnabled(!bulkMatchTargetIds_(selectedMovieIds_()).isEmpty());
}

void MainWindow::showBulkMatchDialog_(const QStringList& movieIds)
{
    const QStringList targetIds = bulkMatchTargetIds_(movieIds);
    if (targetIds.isEmpty()) return;
    if (!m_controller->libraryOpen()) {
        QMessageBox::information(this, tr("Match on TMDb"),
            tr("Open a library before matching titles."));
        return;
    }
    if (!m_tmdb || !m_tmdb->hasApiKey()) {
        QMessageBox::information(this, tr("Match on TMDb"),
            tr("TMDb is not configured. Set your TMDb API key in Settings first."));
        return;
    }

    // Resolve ids to full Movie objects from the list model.
    QList<Movie> movies;
    movies.reserve(targetIds.size());
    for (const QString& id : targetIds)
        if (const Movie* m = m_controller->listModel()->find(id))
            movies.append(*m);
    if (movies.isEmpty()) return;

    BulkTmdbMatchDialog dlg(m_tmdb, movies, this);
    if (dlg.exec() == QDialog::Accepted) {
        const auto matches = dlg.matches();
        if (!matches.isEmpty())
            m_controller->applyTmdbMatches(matches, dlg.downloadPosters());
    }
}

void MainWindow::onBulkMatchStateChanged_()
{
    if (m_controller->bulkMatchInProgress()) {
        if (!m_bulkProgressDlg) {
            m_bulkProgressDlg = new QProgressDialog(this);
            m_bulkProgressDlg->setWindowTitle(tr("Matching on TMDb…"));
            m_bulkProgressDlg->setWindowModality(Qt::WindowModal);
            m_bulkProgressDlg->setCancelButton(nullptr);
            m_bulkProgressDlg->setMinimumDuration(0);
        }
        // Local pointer + setValue() LAST (see onImportStateChanged_): the worker
        // can finish re-entrantly inside setValue()'s processEvents() and null
        // m_bulkProgressDlg, so nothing must touch the member afterwards.
        QProgressDialog* dlg = m_bulkProgressDlg;
        dlg->setLabelText(m_controller->bulkMatchStage());
        dlg->setMaximum(std::max(1, m_controller->bulkMatchTotal()));
        dlg->show();
        dlg->setValue(m_controller->bulkMatchCurrent());
    } else if (m_bulkProgressDlg) {
        // deleteLater (not delete): a modal QProgressDialog::setValue() spins
        // processEvents() internally, so this close path can be reached while a
        // setValue() call on this very dialog is still on the stack (the worker
        // finishing re-entrantly). Deleting it outright would free the object
        // under that call → access violation. deleteLater defers until the stack
        // unwinds; null the member now so it's not touched again meanwhile.
        m_bulkProgressDlg->close();
        m_bulkProgressDlg->deleteLater();
        m_bulkProgressDlg = nullptr;
    }
}

void MainWindow::showSettingsDialog_()
{
    SettingsDialog dlg(m_settings, this);
    dlg.exec();
}

void MainWindow::showCalendarWindow_()
{
    if (!m_calendarWindow)
        m_calendarWindow = new CalendarWindow(m_controller, m_settings, m_tmdb, this);
    m_calendarWindow->show();
    m_calendarWindow->raise();
    m_calendarWindow->activateWindow();
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
