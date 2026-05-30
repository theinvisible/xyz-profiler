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
│   │   └── TmdbClient.h/.cpp  Async search/movie/configuration; language = app locale
│   ├── models/                Data models for views
│   │   ├── MovieListModel.h/.cpp       QAbstractListModel (roles for cover grid)
│   │   ├── MovieTreeModel.h/.cpp       QAbstractItemModel (list view: box-set tree)
│   │   ├── MovieTableModel.h/.cpp      QAbstractTableModel (flat columns; legacy)
│   │   └── MovieSortProxyModel.h/.cpp  QSortFilterProxyModel for icon-view sorting
│   ├── controllers/           Business logic, signals → UI
│   │   ├── LibraryController.h/.cpp    Library open/import/search/select/TMDb
│   │   └── SettingsController.h/.cpp   QSettings-backed prefs (INI file)
│   └── ui/                    Qt Widgets UI layer (see "Subsystem notes")
│       ├── DarkFusionStyle.h/.cpp      Fusion dark/light/system QPalette setup
│       ├── Theme.h/.cpp                Palette + format/age colours, star/loan accents
│       ├── IconFactory.h/.cpp          SVG → tinted QPixmap/QIcon (QPixmapCache'd)
│       ├── CoverArt.h/.cpp             Generated gradient placeholder posters
│       ├── CoverCache.h                Generation-keyed cover QPixmapCache coherency
│       ├── FlowLayout.h/.cpp           Wrapping layout (genre chips)
│       ├── MainWindow.h/.cpp           QMainWindow: toolbar, splitter, QStackedWidget
│       ├── CoverGridWidget.h/.cpp      QListView IconMode + CoverDelegate (cover tiles)
│       ├── MovieRowDelegate.h/.cpp     QTreeView row painter (cover swatch, badges, stars)
│       ├── MovieDetailWidget.h/.cpp    QScrollArea detail pane (462px, tabbed sections)
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
  - `CoverGridWidget` — `QListView` in IconMode with `CoverDelegate`
    painting 168×300 cover tiles (cover image via `QPixmapCache`,
    LOANED dot / SET badge, two-line title + year·genre footer)
  - List view: `QTreeView` over `MovieTreeModel` + a sort proxy, with
    `MovieRowDelegate` painting the title/format/rating columns (cover
    swatch, loan dot, format badge, star rating). Box-set parents are
    expandable rows with their children nested beneath; right-click the
    header for column visibility
  - `MovieDetailWidget` — `QScrollArea` (462px): fixed header (cover,
    title, meta, genre chips, rating, TMDb button) + tabbed sections
    (Overview / Cast & Crew / Tech / Notes). Cover + icons cached via
    `QPixmapCache`
  - View toggle: Grid View ↔ List View via `QStackedWidget`
  - `Theme` / `DarkFusionStyle` — Fusion + QPalette for Dark/Light/System,
    plus `Theme` palette helpers consumed by the custom painters
- **Dual-mode entry point**: GUI by default, CLI with positional XML arg
- **TMDb integration** (`xyz_tmdb` lib):
  - `TmdbClient` — async v3 read API (search, movie, configuration).
    `search`/`getMovie` send `language=` derived from the app locale
    (`QLocale`), so results match the UI language — not hard-coded en-US
  - `QNetworkDiskCache` for poster thumbnails (the `language` query param
    is part of the URL, so switching locale re-fetches cleanly)
  - `TmdbMatchDialog` with poster thumbnails + TMDb attribution
  - Poster download after match (w500, saved to `covers/<id>f.jpg`).
    Re-matching overwrites that file in place → see cover-cache note
  - Schema migration v2 adds `tmdb_id`
- **User settings** (`SettingsController` + INI): TMDb key, images dir,
  theme, view mode, table columns, sort state
- **Two-phase import wizard**: file pick → background parse → preview
  dialog → progress bar → done
- **Box-set grouping**: parents expandable in the list (tree) view with
  children nested beneath, SET badge in the cover grid
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
- Models, all backed by the same `QList<Movie>`: `MovieListModel`
  (role-based, cover grid `QListView`) and `MovieTreeModel` (the
  list/tree view with box-set hierarchy); `MovieTableModel` /
  `MovieSortProxyModel` are retained for sorting. Custom painting lives
  in `CoverDelegate` (grid) and `MovieRowDelegate` (tree rows).
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

## Subsystem notes (non-obvious behaviour)

Things that cost time to rediscover. When you touch one of these, start here.

### Cover images & QPixmapCache coherency

- Cover files live at `<library>/covers/<id>f.jpg` (TMDb posters) or in the
  DP4 images dir (imported scans). The path is stored in
  `Movie::coverFrontPath`.
- Three places paint covers and cache the scaled pixmap: `CoverDelegate::paint`
  (`CoverGridWidget`), `MovieRowDelegate::rowCover`, and
  `MovieDetailWidget::populateHeader_`.
- **Gotcha:** re-matching on TMDb overwrites `covers/<id>f.jpg` *in place*, so
  the path never changes. A cache keyed on the path alone keeps serving the old
  artwork (this is why a re-match left the previous, English cover on screen).
  Fixed via `ui/CoverCache.h`: a per-path *generation* counter folded into the
  cache key. `LibraryController::downloadTmdbPoster_` emits `coverUpdated(path)`;
  `MainWindow` calls `CoverCache::bump(path)` and repaints the views + detail
  pane. **Always build cover-cache keys via `CoverCache::key(path, variant)`**
  — never raw path strings — and `bump()` on any in-place file change.
- Deliberately **no `stat()` per paint** (mtime-in-key was the alternative): the
  cover dir may sit on a network drive, and a filesystem hit per tile per repaint
  would stutter the perf-tuned grid scrolling. The generation map is an in-memory
  hash probe, GUI-thread only (no locking).

### TMDb language

- `TmdbClient` holds `m_language`, defaulted in the ctor from
  `defaultTmdbLanguage()` = `QLocale().name()` with `_`→`-` (e.g. `de_AT` →
  `de-AT`). `search()` and `getMovie()` pass it as `language=`. TMDb falls back
  to the base language when a region variant is missing, so `de-AT` still yields
  German text.
- Mirrors how `main.cpp` picks the UI translation from `QLocale()`. There is no
  separate language *setting* yet; if one is added to `SettingsController`, push
  it via `TmdbClient::setLanguage()` (same pattern as the `setApiKey` wiring in
  `main.cpp`).

## Useful references

- DVD Profiler 4 XML schema (community-mirrored, no longer on invelos.com)
- TMDb API docs: <https://developer.themoviedb.org/reference/intro/getting-started>
- Qt6 Widgets docs: <https://doc.qt.io/qt-6/qtwidgets-index.html>
