#include "MainWindow.h"

#include "controllers/LibraryController.h"
#include "controllers/SettingsController.h"
#include "models/MovieTableModel.h"
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
#include <QTableView>
#include <QToolBar>
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
    m_movieCount->setStyleSheet(QStringLiteral("QLabel { opacity: 0.7; }"));
    tb->addWidget(m_movieCount);

    tb->addSeparator();
    tb->addAction(tr("Settings…"), this, &MainWindow::showSettingsDialog_);
}

void MainWindow::buildCentralWidget_()
{
    auto* splitter = new QSplitter(Qt::Horizontal);

    // Left: stacked view (grid / table)
    m_viewStack = new QStackedWidget;

    m_coverGrid = new CoverGridWidget;
    m_coverGrid->setModel(m_controller->sortProxy());
    m_viewStack->addWidget(m_coverGrid);

    // Table view
    m_tableSortProxy = new QSortFilterProxyModel(this);
    m_tableSortProxy->setSourceModel(m_controller->tableModel());
    m_tableSortProxy->setSortRole(Qt::UserRole);
    m_tableSortProxy->setSortLocaleAware(true);

    m_tableView = new QTableView;
    m_tableView->setModel(m_tableSortProxy);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableView->setSortingEnabled(true);
    m_tableView->setAlternatingRowColors(true);
    m_tableView->verticalHeader()->setVisible(false);
    m_tableView->verticalHeader()->setDefaultSectionSize(26);
    m_tableView->horizontalHeader()->setStretchLastSection(true);
    m_tableView->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tableView->setShowGrid(false);

    connect(m_tableView->horizontalHeader(), &QWidget::customContextMenuRequested,
            this, [this](const QPoint& pos) {
        Q_UNUSED(pos);
        QMenu menu;
        for (int col = 0; col < MovieTableModel::ColumnCount; ++col) {
            auto* act = menu.addAction(
                m_controller->tableModel()->headerData(col, Qt::Horizontal).toString());
            act->setCheckable(true);
            act->setChecked(!m_tableView->isColumnHidden(col));
            connect(act, &QAction::toggled, this, [this, col](bool visible) {
                m_tableView->setColumnHidden(col, !visible);
                // Persist
                QStringList visCols;
                for (int c = 0; c < MovieTableModel::ColumnCount; ++c) {
                    if (!m_tableView->isColumnHidden(c))
                        visCols << QString::number(c);
                }
                m_settings->setVisibleTableColumns(visCols.join(QStringLiteral(";")));
            });
        }
        menu.exec(QCursor::pos());
    });

    setupTableColumnVisibility_();
    m_viewStack->addWidget(m_tableView);

    splitter->addWidget(m_viewStack);

    // Right: detail pane
    m_detailPane = new MovieDetailWidget;
    splitter->addWidget(m_detailPane);

    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 0);
    splitter->setSizes({940, 460});

    setCentralWidget(splitter);

    // Item activation
    connect(m_coverGrid, &CoverGridWidget::movieClicked,
            m_controller, &LibraryController::selectMovie);
    connect(m_tableView, &QAbstractItemView::clicked, this,
            [this](const QModelIndex& idx) {
        const auto srcIdx = m_tableSortProxy->mapToSource(idx);
        const QString id = m_controller->tableModel()->movieIdAtRow(srcIdx.row());
        if (!id.isEmpty()) m_controller->selectMovie(id);
    });

    // TMDb from detail pane
    connect(m_detailPane, &MovieDetailWidget::tmdbSearchRequested,
            m_controller, &LibraryController::searchSelectedOnTmdb);
}

void MainWindow::buildStatusBar_()
{
    statusBar()->showMessage(m_controller->statusMessage());
}

void MainWindow::setupTableColumnVisibility_()
{
    const QString saved = m_settings->visibleTableColumns();
    if (saved.isEmpty()) return;

    const QStringList visible = saved.split(QStringLiteral(";"));

    // Check if stored as column indices (digits) or role names (legacy)
    bool numeric = !visible.isEmpty() && visible.first().at(0).isDigit();

    if (numeric) {
        QSet<int> visCols;
        for (const auto& s : visible) visCols.insert(s.toInt());
        for (int c = 0; c < MovieTableModel::ColumnCount; ++c)
            m_tableView->setColumnHidden(c, !visCols.contains(c));
    } else {
        // Legacy role-name format from QML era — show default columns
        static const QSet<int> defaults = {
            MovieTableModel::Title, MovieTableModel::Year,
            MovieTableModel::Runtime, MovieTableModel::Format,
            MovieTableModel::Rating, MovieTableModel::Director
        };
        for (int c = 0; c < MovieTableModel::ColumnCount; ++c)
            m_tableView->setColumnHidden(c, !defaults.contains(c));
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
    // Don't open the dialog while the search is still running —
    // exec() blocks and a second invocation would stack a new dialog.
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

    if (dlg.exec() == QDialog::Accepted && dlg.selectedTmdbId() > 0) {
        m_controller->pickTmdbMatch(dlg.selectedTmdbId());
    } else {
        m_controller->clearTmdbCandidates();
    }
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
        this,
        tr("Import DVD Profiler Collection.xml"),
        {},
        tr("DVD Profiler XML (Collection.xml *.xml)"));
    if (file.isEmpty()) return;
    m_controller->beginImport(file, m_settings->imagesDirectory());
}

void MainWindow::showSettingsDialog_()
{
    SettingsDialog dlg(m_settings, this);
    dlg.exec();
}

} // namespace xyz
