# XYZ-Profiler

A Qt6/C++ desktop application for managing a personal DVD/Blu-ray/UHD
collection. Built as a replacement for the discontinued **DVD Profiler 4**,
with one-way migration from its `Collection.xml` export.

Primary target: **Windows** (where Profiler 4 ran). Linux is a co-equal
target — Qt6 makes that essentially free as long as we stay platform-neutral.

## Stack

- **Qt 6.5+**, C++20, CMake (≥ 3.21)
- **UI: QML / Qt Quick** (Quick Controls 2, Material or Universal style).
  Widgets are only used where QML has no good answer (rare). See conventions.
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
│   ├── main.cpp                Currently a CLI smoke-test; becomes the QML host
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
│   │   ├── TmdbClient.h/.cpp  Async search/movie/configuration
│   ├── models/                QML-facing data presentation
│   │   └── MovieListModel.h/.cpp  QAbstractListModel with named roles
│   ├── controllers/           QML-facing imperative API
│   │   ├── LibraryController.h/.cpp     Q_INVOKABLE actions, selection state,
│   │   │                                 two-phase import (preview + commit),
│   │   │                                 TMDb search + match
│   │   └── SettingsController.h/.cpp    QSettings-backed prefs (TMDb key,
│   │                                     images dir, theme)
│   └── qml/                   QML view files (bundled via qt_add_qml_module)
│       ├── Main.qml                ApplicationWindow shell + header / footer / split
│       ├── CoverGrid.qml           GridView with lazy cover thumbnails + badges
│       ├── MovieDetail.qml         Right-side detail pane (cast, crew, audio, …)
│       ├── TmdbMatchDialog.qml     TMDb candidate picker
│       ├── ImportPreviewDialog.qml Two-phase import confirmation
│       └── SettingsDialog.qml      Persistent user settings editor
└── tests/
    ├── CMakeLists.txt
    ├── test_collection_xml_reader.cpp   QtTest, registered with ctest
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

All Phase-1-through-Phase-6 modules are now in place. The roadmap below
tracks remaining items.

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
different Qt install. Required modules: Core / Gui / Widgets / Quick / Qml /
Sql / Network / Xml / Test.

## Status

**Done**

- CMake skeleton (Win + Linux), C++20, AUTOMOC
- Modular domain layer in `src/domain/` (header-only `xyz_domain` INTERFACE
  lib) — `MediaItem` base + `Movie` specialisation. The base carries all
  fields that are reusable for non-film media; a future `Game` type slots
  in beside `Movie` without touching the importer or repository contracts.
- `CollectionXmlReader` — parses real-world DP4 Collection.xml exports:
  - Identity: `ID`, parsed UPC siblings (base / variant / locality / type),
    formatted `UPC`
  - Titles + `DistTrait` (edition label)
  - Provenance: production year, release date, up to 3 `CountryOfOrigin*`
  - Classification: genres, tags, full `RatingInfo` (system/value/age/
    variant/details), `CaseType` + slip-cover, regions
  - People: full actor and credit attribute set (middle name, birth year,
    credited-as, voice/uncredited/puppeteer flags)
  - Companies: `Studios`, `MediaCompanies`
  - User-managed: `CollectionType` (Owned/Wishlist),
    `CollectionNumber`, `CountAs`, `WishPriority`, `CustomFields`,
    `Locks` (collapsed to enabled-only)
  - Descriptions: overview, notes, multi-line `EasterEggs`
  - Audio: `<Audio>/<AudioTrack>` with both prefixed (`AudioContent/...`)
    and unprefixed (`Content/...`) child schemas, plus attribute style
  - Subtitles
  - Discs: full `<Discs>/<Disc>` with side-A/B descriptions, IDs, labels,
    dual-layered/-sided flags, location, slot
  - Video format: aspect ratio, video standard, `ColorFormat` collapsed
    to one of Color/BlackAndWhite/Colorized/Mixed, `Dimensions` collapsed
    to one of 2D/3DAnaglyph/3DBluRay, letterbox/16x9/dual-sided/-layered
  - Features: 24 boolean flags stored as enabled-only name list,
    plus `OtherFeatures` free text
  - `MediaBanners` (front/back)
  - `BoxSet`: `<Parent>` (child→parent link) and `<Contents>/<Content>`
    (parent→children list)
  - Purchase: full `PurchaseInfo` with `MonetaryAmount` price (currency
    triple), place/type/website, gift-from
  - `SRP` (suggested retail price, same `MonetaryAmount` shape)
  - `LoanInfo` (loaned/due/user) + `Events` history with timestamps
  - `Review` (own star ratings)
  - Timestamps: `ProfileTimestamp`, `LastEdited`
  - Cover resolution from `Images/` directory
  - Binary `QIODevice` open so `QXmlStreamReader` honours the
    `windows-1252` encoding declaration in real exports
  - Robust against unknown elements (forward-compatible)
- Modular `Importer` base class — `DvdProfilerXmlImporter` is the first
  concrete implementation, additional source formats slot in as siblings
  under `src/importers/` without touching the persistence layer
- SQLite persistence (`xyz_db` lib):
  - `Database` wrapper with WAL journal mode + foreign-key enforcement
  - Versioned `Migrations` runner (schema_version table, idempotent)
  - Schema v1: main `movies` table mirroring all flat `Movie` fields,
    plus 16 child tables for collections (genres, actors, credits,
    audio_tracks, discs, events, custom_fields, box_set_children, ...)
  - FTS5 virtual table `movies_fts` over title, original_title, overview,
    notes, easter_eggs, actors (joined names), credits, studios — ranked
    via SQLite's bm25
  - `MovieRepository` with `insert`, `bulkInsert` (transactional),
    `count`, `getById`, `getAll`, `search(query, limit)`
  - Cover paths stored relative to a configurable library root
- 47 unit tests across two binaries (importer + repository), plus the
  sample-data end-to-end test
- ctest configured to inject Qt's `bin/` into PATH on Windows so test exes
  don't hang waiting for a missing-DLL dialog
- **QML UI** (Material Dark theme):
  - `Main.qml` — `ApplicationWindow` shell with header (search bar + open
    library + movie count) and footer (status bar). Starts maximized.
  - `CoverGrid.qml` — `GridView` of 180×270 tiles. Lazy-loaded cover
    images with placeholder fallback; LOANED and SET (box-set parent)
    badges; click → selectMovie(id).
  - `MovieDetail.qml` — flat right-side pane (460 wide) showing title /
    year / format / rating / cast / crew / audio tracks / discs /
    technical / purchase / loan / tags. All bindings come from
    `LibraryController.selected*` properties for a flat render path.
  - `MovieListModel` exposes role names (title, year, format, coverPath,
    director, isLoaned, isBoxSetParent, …) for the grid delegate.
  - `LibraryController` is an application-owned QML singleton registered
    via `qmlRegisterSingletonInstance` (QML_SINGLETON's default-factory
    behavior would have given QML its own fresh instance, bypassing the
    one already wired to the DB).
- **Dual-mode entry point**: `xyz-profiler.exe` launches the GUI by
  default; passing a Collection.xml positional arg keeps the original CLI
  flow for batch / scripted use.
- **TMDb integration** (`xyz_tmdb` lib):
  - `TmdbClient` — async wrapper around TMDb's v3 read API
    (`search/movie`, `movie/{id}`, `configuration`); JSON parsed via
    `QJsonDocument`, no SDK dependency
  - Shared `QNetworkDiskCache` at `QStandardPaths::CacheLocation/network`
    used by both `TmdbClient` and the QML image loader (via a
    `QQmlNetworkAccessManagerFactory`) — poster thumbnails persist across
    restarts
  - Schema migration v2 adds `tmdb_id` to `movies` with an index
  - `LibraryController` exposes `searchSelectedOnTmdb` / `pickTmdbMatch`
    invokables and a `tmdbCandidates` model
  - `TmdbMatchDialog.qml` shows poster + title + year + overview for
    each hit; click → persisted to DB, indicator appears in detail pane
  - API key from `TMDB_API_KEY` env var; missing key gracefully disables
    the buttons rather than crashing (proper settings UI deferred to a
    later phase)
- **User settings** (`SettingsController` + `xyz-profiler.ini` under
  `AppConfigLocation`): TMDb API key (overrides `TMDB_API_KEY` env var),
  default cover-images directory, theme (Dark/Light/System — bound to
  `Material.theme` so switches apply instantly without restart). Exposed
  to QML as a singleton; editable via the toolbar Settings… dialog.
- **Two-phase import wizard**: file pick → background parse (cancellable)
  → `ImportPreviewDialog` showing count + first 8 titles + chosen images
  dir → on confirm runs the per-row DB write with the existing progress
  bar; cancel discards the parsed data without touching the DB.
- **Internationalisation**: English + German via `qt_add_translations`;
  system locale auto-detected, falls back to English
- **Packaging**: Inno Setup script + GitHub Actions workflow that builds,
  tests, ZIPs, and attaches signed installers to tagged releases

**Validated against** the user's real 369-entry DP4 export — all movies
persisted to a 10 MB SQLite file, FTS5 search returns expected hits for
title queries ("matrix" → 7), actor queries ("adrien brody" → 2),
and director surname queries ("wachowski" → 7). GUI shell visually
verified against the same library.

## Roadmap

Roughly in build order — each chunk should ship green tests.

### 1. Reader hardening — DONE
- [x] Parse `<Audio>` / `<AudioTrack>`, `<Subtitles>`, `<Discs>/<Disc>`
- [x] Parse box-set parent/child relations
- [x] Parse `<Tags>`, `<CustomFields>` (defensive — empty in real export)
- [x] Validate against a real export — covered every element the user's
      DP4 collection ships, including loan history, FSK ratings, 3D
      releases, multi-disc box sets, and cp1252-encoded German content

### 2. SQLite persistence (`src/db/`) — DONE
- [x] Schema migrations (version table + idempotent migration runner)
- [x] `MovieRepository` — insert / bulk / count / getById / getAll
- [x] FTS5 virtual table over title/actors/credits/studios/overview/notes
- [x] Cover paths stored as relative; resolved against a library root

### 3. QML UI shell (`src/qml/`) — DONE
- [x] Switch `main.cpp` to `QGuiApplication` + `QQmlApplicationEngine`
- [x] `Main.qml` — ApplicationWindow, header search bar, footer status bar
- [x] `CoverGrid.qml` — `GridView` with lazy-loaded cover thumbnails +
      LOANED / SET badges
- [x] `MovieDetail.qml` — full info pane (cast, crew, technical, notes,
      purchase, loan, …)
- [x] Material Dark theme — explicit theme toggle deferred to Phase 6 settings

### 4. C++ ↔ QML glue — MOSTLY DONE (alongside Phase 3)
- [x] `MovieListModel : QAbstractListModel` — backed by repository with
      role names for QML
- [x] `LibraryController : QObject` — `Q_INVOKABLE` actions for QML
      (openLibrary, search, refresh, selectMovie, importDvdProfilerXml)
- [ ] `MovieFilterProxyModel : QSortFilterProxyModel` — deferred; FTS5
      search in the repository covers the v1 search case already

### 5. TMDb integration (`src/tmdb/`) — DONE for matching; refresh deferred
- [x] `TmdbClient` — async `search/movie`, `movie/{id}`, image config
- [x] Disk image cache (`QStandardPaths::CacheLocation`) — shared by
      TmdbClient and the QML `Image` loader via a
      `QQmlNetworkAccessManagerFactory`
- [x] Match dialog (QML) — auto-shows on candidates, posters via TMDb
      thumbnails (w185), pick → persisted to DB
- [x] Store `tmdb_id` on `Movie` (schema migration v2)
- [ ] Refresh-from-TMDb action (overwrite local metadata with TMDb data) —
      deferred to a later phase; needs UX for "which fields to overwrite"

### 6. Settings & first-run — DONE
- [x] `QSettings`-backed config: TMDb API key, images dir, theme.
      Library DB path is intentionally NOT a setting — it's auto-managed
      under `AppLocalDataLocation`.
- [x] Import wizard: file pick → background parse → preview dialog
      (count + sample titles) → confirm → progress dialog → done

### Post-MVP (explicitly out of scope for v1)
- Loan tracking (who borrowed which disc)
- Barcode scanning (EAN via webcam / phone companion)
- Multi-user / sync

## Conventions

### QML-first

The UI is **QML wherever possible**. Reasons:

- Modern look-and-feel with minimal code; trivial dark mode / theming
- `GridView` with lazy delegate instantiation handles 10k+ cover thumbnails
  fluidly — would need significant `QStyledItemDelegate` work in Widgets
- Touch / gesture friendly for future tablet builds

**Concretely:**

- `main()` uses `QGuiApplication` + `QQmlApplicationEngine` (not
  `QApplication`). The smoke-test CLI in `src/main.cpp` is a temporary
  scaffold and will be replaced.
- Business logic lives in C++ (`QObject` subclasses, models). QML calls
  into it via `Q_INVOKABLE`, properties, and signals — never the reverse.
- Models exposed to QML are `QAbstractListModel` subclasses with named
  roles, registered with `qmlRegisterType` or via context properties.
- Custom delegates and components go under `src/qml/components/`.
- Widgets are only acceptable for: (a) native file dialogs where
  `QFileDialog` is materially better than `Qt.labs.platform`, and
  (b) tray-icon integration. Justify in a comment when used.

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
- Use `QTEST_GUILESS_MAIN` for non-UI tests, `QTEST_MAIN` for QML tests
  (when we add them).
- Test data files live under `tests/data/`, path injected via
  `TEST_DATA_DIR` compile definition.

### Commits / branches

- Conventional Commits (`feat:`, `fix:`, `refactor:`, `test:`, `docs:`).
- Feature branches: `feat/<short-slug>`; PR squash-merges into `main`.
- Every PR must keep ctest green on Windows + Linux.

## Useful references

- DVD Profiler 4 XML schema (community-mirrored, no longer on invelos.com)
- TMDb API docs: <https://developer.themoviedb.org/reference/intro/getting-started>
- Qt6 QML best practices: <https://doc.qt.io/qt-6/qtquick-bestpractices.html>
