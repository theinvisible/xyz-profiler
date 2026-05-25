# XYZ-Profiler

A Qt6/C++ desktop application for managing a personal DVD/Blu-ray/UHD
collection. Built as a replacement for the discontinued **DVD Profiler 4**,
with one-way migration from its `Collection.xml` export.

Primary target: **Windows** (where Profiler 4 ran). Linux is a co-equal
target — Qt6 makes that essentially free as long as we stay platform-neutral.

## Stack

- **Qt 6.5+**, C++20, CMake (≥ 3.21)
- **UI: Qt Widgets** (Fusion style with dark/light/system QPalette +
  stylesheet). Migrated from QML — Widgets are a better fit for this
  data-heavy desktop app (QTableView, QHeaderView, QSplitter all built-in).
- **Persistence:** SQLite via `Qt6::Sql`, FTS5 for full-text search.
- **XML import:** `QXmlStreamReader` (streaming, scales to large collections).
- **Online metadata:** TMDb via `QNetworkAccessManager` (replaces the dead
  Invelos API). No SDK — raw REST + `QJsonDocument` is enough.

## Project layout

```
xyz-profiler/
├── CMakeLists.txt              Top-level: Qt setup, subdirs, ctest
├── src/
│   ├── CMakeLists.txt
│   ├── main.cpp                Dual-mode: GUI (QApplication + MainWindow)
│   │                           or CLI (positional Collection.xml arg)
│   ├── domain/                Header-only data model (xyz_domain INTERFACE lib)
│   │   ├── Person.h           name + middle + birthYear + role + voice/puppeteer/...
│   │   ├── AudioTrack.h       content / format / channels
│   │   ├── Disc.h             side-A/B description, ID, label, dual-layered, location
│   │   ├── CustomField.h      Name / value pair
│   │   ├── MonetaryAmount.h   value + denomination triple (used by Purchase + SRP)
│   │   ├── PurchaseInfo.h     date / place / gift / MonetaryAmount price
│   │   ├── BoxSet.h           parentId + childIds + isParent
│   │   ├── IdMetadata.h       parsed UPC: base / variant / locality / type
│   │   ├── CollectionMembership.h  Owned/Wishlist + isPartOfOwnedCollection
│   │   ├── RatingInfo.h       system / value / age / variant / details (FSK, MPAA)
│   │   ├── VideoFormat.h      aspect / standard / color / dimensions / letterbox …
│   │   ├── LoanInfo.h         loaned / due / user
│   │   ├── Event.h            type / timestamp / note / user (loan history)
│   │   ├── Review.h           own star ratings (film/video/audio/extras)
│   │   ├── MediaItem.h        Media-neutral base — everything reusable for games
│   │   └── Movie.h            Movie : MediaItem (+ runtime, format, audio, video, …)
│   ├── importers/             Source-format importers (xyz_importers lib)
│   │   ├── Importer.h         Abstract base — id, displayName, importFile
│   │   └── dvdprofiler/       DP4 Collection.xml implementation
│   │       ├── DvdProfilerXmlImporter.h
│   │       └── DvdProfilerXmlImporter.cpp
│   ├── db/                    SQLite persistence (xyz_db lib)
│   │   ├── Database.h/.cpp    Connection wrapper, WAL + FK pragmas
│   │   ├── Migrations.h/.cpp  schema_version + idempotent runner
│   │   └── MovieRepository.h/.cpp  insert/bulk/getById/getAll/search (FTS5)
│   ├── tmdb/                  TMDb v3 read API client (xyz_tmdb lib)
│   │   ├── TmdbTypes.h        Candidate / MovieDetails / ImageConfig
│   │   └── TmdbClient.h/.cpp  Async search/movie/configuration
│   ├── models/                Data models for views
│   │   ├── MovieListModel.h/.cpp       QAbstractListModel (roles for cover grid)
│   │   ├── MovieTableModel.h/.cpp      QAbstractTableModel (columns for table view)
│   │   └── MovieSortProxyModel.h/.cpp  QSortFilterProxyModel for icon-view sorting
│   ├── controllers/           Business logic, signals → UI
│   │   ├── LibraryController.h/.cpp    Library open/import/search/select/TMDb
│   │   └── SettingsController.h/.cpp   QSettings-backed prefs (INI file)
│   └── ui/                    Qt Widgets UI layer
│       ├── DarkFusionStyle.h/.cpp      Fusion dark/light/system theme setup
│       ├── MainWindow.h/.cpp           QMainWindow: toolbar, splitter, stacked views
│       ├── CoverGridWidget.h/.cpp      QListView IconMode + custom QStyledItemDelegate
│       ├── MovieDetailWidget.h/.cpp    QScrollArea detail pane (460px, all sections)
│       ├── TmdbMatchDialog.h/.cpp      TMDb candidate picker with poster thumbnails
│       ├── ImportPreviewDialog.h/.cpp  Two-phase import confirmation
│       └── SettingsDialog.h/.cpp       TMDb key, images dir, theme editor
└── tests/
    ├── CMakeLists.txt
    ├── test_dvdprofiler_xml_importer.cpp   QtTest, registered with ctest
    ├── test_movie_repository.cpp
    └── data/
        └── sample_collection.xml
```

The split is intentional: `src/domain/` carries only data structs and depends
on `Qt6::Core` plus nothing else. Importers, persistence, and UI all consume
it. This keeps the door open for managing **video games** in the same app
later — `MediaItem` already covers what films and games share (title, year,
genres, credits/developers, tags, purchase info, covers, custom fields), and
a future `Game : MediaItem` would slot in beside `Movie` without disturbing
the importer or repository contracts.

## Build & test

```powershell
# Configure (CLion does this automatically when opening the folder)
cmake -S . -B build
cmake --build build

# Run tests
ctest --test-dir build --output-on-failure

# Run the smoke-test CLI against a real export
.\build\xyz-profiler.exe path\to\Collection.xml --images path\to\Images
```

The top-level `CMakeLists.txt` auto-detects common Qt install locations
(currently `H:/Qt/6.11.1/msvc2022_64`, plus `C:/Qt/...` fallbacks). Override
with `-DCMAKE_PREFIX_PATH=...` or the matching environment variable for a
different Qt install. Required modules: Core / Concurrent / Gui / Network /
Widgets / Sql / Svg / Test / LinguistTools.

## Status

**Done**

- CMake skeleton (Win + Linux), C++20, AUTOMOC
- Modular domain layer in `src/domain/` (header-only `xyz_domain` INTERFACE
  lib) — `MediaItem` base + `Movie` specialisation
- `DvdProfilerXmlImporter` — parses real-world DP4 Collection.xml exports
  with full field coverage (audio, discs, box sets, ratings, loans, purchase,
  features, video format, etc.). Binary `QIODevice` open honours the
  `windows-1252` encoding declaration.
- Modular `Importer` base class — additional source formats slot in as
  siblings under `src/importers/`
- SQLite persistence (`xyz_db` lib): WAL mode, foreign keys, versioned
  migrations (v1: full schema + FTS5, v2: tmdb_id), `MovieRepository`
  with CRUD + bulk + FTS5 search (bm25 ranking)
- **Qt Widgets UI** (Fusion dark theme):
  - `MainWindow` — `QMainWindow` with toolbar (import, view toggle,
    search field, movie count, settings), `QSplitter` (views + detail),
    `QStatusBar`
  - `CoverGridWidget` — `QListView` in IconMode with custom
    `QStyledItemDelegate` painting 180×270 tiles (cover image with
    `QPixmapCache`, LOANED/SET badges, title+year footer)
  - `QTableView` with `QSortFilterProxyModel` — sortable columns,
    right-click header for column visibility, box-set parent rows bold,
    child rows indented
  - `MovieDetailWidget` — `QScrollArea` (460px) with all movie sections
    (title, cover, TMDb, loan, overview, cast, crew, audio, discs,
    technical, purchase, tags). Cover cached via `QPixmapCache`,
    all sections as static QLabels (no dynamic widget creation)
  - View toggle: Grid View ↔ List View via `QStackedWidget`
  - `DarkFusionStyle` — Fusion + QPalette for Dark/Light/System themes
- **Dual-mode entry point**: GUI by default, CLI with positional XML arg
- **TMDb integration** (`xyz_tmdb` lib):
  - `TmdbClient` — async v3 read API (search, movie, configuration)
  - `QNetworkDiskCache` for poster thumbnails
  - `TmdbMatchDialog` with poster thumbnails + TMDb attribution
  - Poster download after match (w500, saved to `covers/` dir)
  - Schema migration v2 adds `tmdb_id`
- **User settings** (`SettingsController` + INI): TMDb key, images dir,
  theme, view mode, table columns, sort state
- **Two-phase import wizard**: file pick → background parse → preview
  dialog → progress bar → done
- **Box-set grouping**: children auto-sorted after parent, indented in
  table view, SET badge in cover grid
- **Internationalisation**: English + German via `qt_add_translations`
- **Packaging**: Inno Setup script + GitHub Actions workflow

**Validated against** the user's real 369-entry DP4 export. 47 unit tests
across two binaries (importer + repository).

## Roadmap

### Post-MVP (explicitly out of scope for v1)
- Refresh-from-TMDb action (overwrite local metadata)
- Loan tracking UI (data already imported)
- Barcode scanning (EAN via webcam / phone companion)
- Multi-user / sync

## Conventions

### Widgets-first

The UI uses **Qt Widgets** exclusively. Reasons:

- `QTableView` / `QHeaderView` / `QSortFilterProxyModel` provide sorting,
  column visibility, and column reordering with zero custom code
- `QStyledItemDelegate::paint()` is fast — no binding engine overhead
- `QSplitter`, `QStackedWidget`, `QProgressDialog` just work
- Fusion + QPalette gives clean dark/light theming via `DarkFusionStyle`

**Concretely:**

- `main()` uses `QApplication` + `MainWindow`
- Business logic lives in controllers (`QObject` subclasses). The UI
  connects to signals and calls public methods — no `Q_PROPERTY` fan-out,
  no `Q_INVOKABLE` annotations needed.
- Two models: `MovieListModel` (role-based, for the cover grid's
  `QListView` in IconMode) and `MovieTableModel` (column-based, for
  `QTableView`). Both backed by the same `QList<Movie>` data.
- Controllers expose `const Movie&` directly — no QVariantMap conversion.
- Dialogs are `QDialog` subclasses shown via `exec()`.

### Code

- C++20, no exceptions in the hot path (Qt convention). Errors flow back
  as `Result`-style structs or via signals.
- `QStringLiteral` / `u"…"` for string literals; avoid implicit `QString`
  construction from `const char*`.
- Headers under `src/<module>/`, include as `"module/Header.h"` from the
  module's public `target_include_directories`.
- One class per file; filename matches class name.

### Tests

- QtTest, one test binary per module under `tests/`.
- Each binary registered with ctest via `add_test`.
- Use `QTEST_GUILESS_MAIN` for non-UI tests.
- Test data files live under `tests/data/`, path injected via
  `TEST_DATA_DIR` compile definition.

### Commits / branches

- Conventional Commits (`feat:`, `fix:`, `refactor:`, `test:`, `docs:`).
- Feature branches: `feat/<short-slug>`; PR squash-merges into `main`.
- Every PR must keep ctest green on Windows + Linux.

## Useful references

- DVD Profiler 4 XML schema (community-mirrored, no longer on invelos.com)
- TMDb API docs: <https://developer.themoviedb.org/reference/intro/getting-started>
- Qt6 Widgets docs: <https://doc.qt.io/qt-6/qtwidgets-index.html>
