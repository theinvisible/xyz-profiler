#include "MainWindow.h"

#include "ui/DarkFusionStyle.h"

#include "controllers/LibraryController.h"
#include "controllers/SettingsController.h"
#include "models/MovieListModel.h"
#include "models/MovieTreeModel.h"
#include "tmdb/TmdbClient.h"
#include "ui/CoverGridWidget.h"
#include "ui/ImportPreviewDialog.h"
#include "ui/MovieDetailWidget.h"
#include "ui/SettingsDialog.h"
#include "ui/TmdbMatchDialog.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QFileDialog>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QProgressDialog>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QToolBar>
#include <QTreeView>
#include <QVBoxLayout>

namespace xyz {

MainWindow::MainWindow(LibraryController* controller,
                       SettingsController* settings,
                       TmdbClient* tmdb,
                       QWidget* parent)
    : QMainWindow(parent),
      m_controller(controller),
      m_settings(settings),
      m_tmdb(tmdb)
{
    setWindowTitle(QStringLiteral("xyz-profiler"));
    resize(1400, 900);

    buildToolBar_();
    buildCentralWidget_();
    buildStatusBar_();
    connectController_();
    connectSettings_();

    setStyleSheet(DarkFusionStyle::darkStyleSheet());

    switchView_(m_settings->viewMode());
    onMoviesChanged_();
}

// ---------------------------------------------------------------------------
// Build
// ---------------------------------------------------------------------------

void MainWindow::buildToolBar_()
{
    auto* tb = addToolBar(tr("Main"));
    tb->setMovable(false);
    tb->setIconSize(QSize(16, 16));

    tb->addAction(tr("Import Collection.xml…"), this, &MainWindow::showImportDialog_);
    tb->addSeparator();

    auto* viewGroup = new QActionGroup(this);
    m_gridAction = viewGroup->addAction(tr("Grid View"));
    m_gridAction->setCheckable(true);
    m_listAction = viewGroup->addAction(tr("List View"));
    m_listAction->setCheckable(true);
    tb->addActions(viewGroup->actions());

    connect(m_gridAction, &QAction::triggered, this,
            [this]() { m_settings->setViewMode(QStringLiteral("grid")); });
    connect(m_listAction, &QAction::triggered, this,
            [this]() { m_settings->setViewMode(QStringLiteral("list")); });

    tb->addSeparator();

    m_searchField = new QLineEdit;
    m_searchField->setPlaceholderText(
        tr("Search title, actor, director, studio, overview…"));
    m_searchField->setClearButtonEnabled(true);
    m_searchField->setMaximumWidth(400);
    tb->addWidget(m_searchField);

    connect(m_searchField, &QLineEdit::returnPressed, this,
            [this]() { m_controller->search(m_searchField->text()); });
    connect(m_searchField, &QLineEdit::textChanged, this,
            [this](const QString& t) { if (t.isEmpty()) m_controller->refresh(); });

    auto* spacer = new QWidget;
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    tb->addWidget(spacer);

    m_movieCount = new QLabel;
    tb->addWidget(m_movieCount);

    tb->addSeparator();
    tb->addAction(tr("Settings…"), this, &MainWindow::showSettingsDialog_);
}

void MainWindow::buildCentralWidget_()
{
    auto* splitter = new QSplitter(Qt::Horizontal);
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
    m_treeView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_treeView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_treeView->setSortingEnabled(true);
    m_treeView->setAlternatingRowColors(true);
    m_treeView->setRootIsDecorated(true);
    m_treeView->setItemsExpandable(true);
    m_treeView->setAnimated(true);
    m_treeView->setUniformRowHeights(true);
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
    m_treeView->sortByColumn(MovieTreeModel::PurchaseDate, Qt::DescendingOrder);
    m_viewStack->addWidget(m_treeView);

    splitter->addWidget(m_viewStack);

    // ---- Detail pane (right) -----------------------------------------------
    m_detailPane = new MovieDetailWidget;
    splitter->addWidget(m_detailPane);

    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 0);
    splitter->setSizes({940, 460});
    setCentralWidget(splitter);

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
    statusBar()->showMessage(m_controller->statusMessage());
}

void MainWindow::setupTreeColumnVisibility_()
{
    const QString saved = m_settings->visibleTableColumns();

    static const QSet<int> defaults = {
        MovieTreeModel::Title, MovieTreeModel::Year,
        MovieTreeModel::Runtime, MovieTreeModel::Format,
        MovieTreeModel::Rating, MovieTreeModel::Director,
        MovieTreeModel::PurchaseDate
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
            [this]() { statusBar()->showMessage(m_controller->statusMessage()); });
    connect(m_controller, &LibraryController::importStateChanged,
            this, &MainWindow::onImportStateChanged_);
    connect(m_controller, &LibraryController::tmdbStateChanged,
            this, &MainWindow::onTmdbStateChanged_);
}

void MainWindow::connectSettings_()
{
    connect(m_settings, &SettingsController::viewModeChanged, this,
            [this]() { switchView_(m_settings->viewMode()); });
    connect(m_settings, &SettingsController::themeNameChanged, this,
            [this]() {
        setStyleSheet(m_settings->themeName() == QLatin1String("Dark")
                          ? DarkFusionStyle::darkStyleSheet()
                          : QString());
    });
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void MainWindow::onMoviesChanged_()
{
    m_movieCount->setText(tr("%1 movies").arg(m_controller->movieCount()));
    m_searchField->setEnabled(m_controller->movieCount() > 0);
}

void MainWindow::onSelectionChanged_()
{
    if (m_controller->hasSelection())
        m_detailPane->updateFromMovie(m_controller->selectedMovie());
    else
        m_detailPane->clearSelection();
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
    m_gridAction->setChecked(!isList);
    m_listAction->setChecked(isList);
}

void MainWindow::showImportDialog_()
{
    const QString file = QFileDialog::getOpenFileName(
        this, tr("Import DVD Profiler Collection.xml"),
        {}, tr("DVD Profiler XML (Collection.xml *.xml)"));
    if (file.isEmpty()) return;
    m_controller->beginImport(file, m_settings->imagesDirectory());
}

void MainWindow::showSettingsDialog_()
{
    SettingsDialog dlg(m_settings, this);
    dlg.exec();
}

} // namespace xyz
