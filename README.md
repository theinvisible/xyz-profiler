# xyz-profiler

A desktop manager for personal **DVD / Blu-ray / UHD collections**, built
as a long-term replacement for the discontinued *DVD Profiler 4*. Imports
your existing DP4 `Collection.xml` export, persists everything in a local
SQLite library, and searches/browses with a native **Qt Widgets** UI
(Fusion, with dark / light / system themes).

![Platform](https://img.shields.io/badge/platform-Windows%2010%2F11%20%2F%20Linux-blue)
![C++20](https://img.shields.io/badge/C%2B%2B-20-orange)
![Qt](https://img.shields.io/badge/Qt-6-41cd52)

## Features

- **One-way import from DVD Profiler 4** — validated against a real
  369-entry collection. Captures every field DP4 exports: audio tracks
  (including DTS-HD / Atmos), discs with side-A/B detail, box-set
  parent/child relations, FSK / MPAA ratings, regions, features, loan
  history, easter eggs, purchase info — see [DP4 schema notes](#dp4-import).
- **SQLite-backed library** with versioned schema migrations, automatic
  WAL journal mode, and an **FTS5 index** over title, original title,
  overview, cast, crew and studios — ranked via SQLite's `bm25`.
- **Qt Widgets UI** — switch between a **cover grid** (`QListView` icon
  mode, custom-painted 168×300 tiles) and a **list/tree view** (`QTreeView`
  with box-set parents expandable over their children, sortable columns,
  toggleable column visibility), plus a tabbed **detail pane**. Cover
  pixmaps are cached; LOANED and box-set badges on tiles. Fusion theme with
  dark / light / system palettes.
- **Add / edit / delete titles** — a tabbed edit dialog mutates a full copy
  of the movie, so untouched fields (cast, discs, box-set membership) survive
  a save. Optional in-dialog TMDb prefill, and pick / preview / clear custom
  front & back cover images.
- **TMDb integration** — async v3 search with poster thumbnails;
  *Match on TMDb* links an existing entry, *Add a title* does a rich
  search / discover lookup (year range, genres, person, rating, language,
  country, runtime, sort) to create a new one, and **bulk match** links many
  movies at once (bounded-concurrent search, per-row override/skip, then an
  optional throttled poster download). Posters save to the library and the
  search language follows the UI locale.
- **Async import wizard** — XML parsing on a worker thread, preview
  dialog (count + sample titles), then a progress bar while rows are
  written to the database. UI thread never blocks.
- **Multilingual** — English + German, locale auto-detected from the OS;
  falls back to English when no translation matches.
- **Cross-platform**: Qt 6 + standard CMake, builds the same on Windows
  and Linux. Cover paths are stored relative to a configurable library
  root so the directory can be moved without re-import.

## Screenshots

> Add screenshots under `docs/screenshots/` and reference them here.

## How it works

| Layer | Module | Role |
|---|---|---|
| Domain | `xyz_domain` (header-only) | Plain-aggregate structs — `Movie`, `MediaItem`, `Person`, `Disc`, `AudioTrack`, `RatingInfo`, … Reusable for non-film media (a future `Game : MediaItem` slots in beside `Movie`). |
| Importers | `xyz_importers` | Abstract `Importer` base + `DvdProfilerXmlImporter`. Source-format-specific code is isolated here; future importers drop in as sibling modules. |
| Persistence | `xyz_db` | `Database` (connection + pragmas), `Migrations` (versioned, idempotent — v1 schema + FTS5, v2 `tmdb_id`, v3 canonical disc format), `MovieRepository` (CRUD incl. `remove`, bulk insert, cheap single-column cover updates, FTS5 search). |
| Metadata | `xyz_tmdb` | Async TMDb v3 read client (`search`/`searchFor`, `movie/{id}`, `configuration`, `discover`). A `QNetworkDiskCache` persists poster thumbnails across runs. Search/detail `language=` follows the app locale. |
| UI | `xyz-profiler` exe | Qt Widgets. Controllers (`LibraryController`, `SettingsController`) are plain `QObject`s the UI connects to. Models: `MovieListModel` (grid), `MovieTreeModel` (box-set tree). Views/widgets: `MainWindow`, `CoverGridWidget`, `MovieRowDelegate`, `MovieDetailWidget`, plus dialogs (TMDb match, bulk match, add title, edit movie, import preview, settings). |

The library DB lives under the per-user AppData directory and is
auto-created on first launch; the GUI never asks for a path.

## DP4 import

Validated against a real DP4 export with the schema quirks it hits in
practice — including the `Audio` → `<AudioContent>/<AudioFormat>/
<AudioChannels>` child names (not `Content/Format/Channels` as some
mirrors document), the `<Discs>/<Disc>` shape with side-A/B fields (not a
flat `<DiscIDs>` list), the `<PurchasePrice>` + `<PurchaseDate>` element
names, and the `<BoxSet><Parent>` / `<Contents><Content>` shape.

The importer opens the file in **binary mode** so `QXmlStreamReader`
honours the `<?xml encoding="windows-1252"?>` declaration — opening with
`QIODevice::Text` translation corrupts non-ASCII bytes.

## Requirements

### Runtime

- Windows 10 1809 (build 17763) or newer, x64 — or Linux
- For TMDb features: a free API key from
  [themoviedb.org](https://www.themoviedb.org/settings/api), set via the
  Settings dialog or the `TMDB_API_KEY` environment variable.

### Build

- Visual Studio 2022 (MSVC v143) on Windows, or a recent g++/clang on Linux
- CMake ≥ 3.21
- Qt 6.5+ (tested with 6.10.2 and 6.11.1) — MSVC 64-bit kit on Windows.
  Required Qt modules: `Core`, `Concurrent`, `Gui`, `Network`, `Widgets`,
  `Sql`, `Svg`, `Test`, `LinguistTools`.

The CMake script auto-detects Qt under `H:/Qt` or `C:/Qt`. To use a
different location, point CMake at it:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.10.2/msvc2022_64"
```

## Building

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target xyz-profiler -j
```

`windeployqt` is invoked automatically as a post-build step, so
`build/xyz-profiler.exe` is launch-ready — no need to put Qt on `PATH`.

To run the test suite:

```powershell
ctest --test-dir build --output-on-failure
```

### CLion

Open the folder, pick a Visual Studio toolchain, and build the
`xyz-profiler` target. The CMake auto-detection picks up Qt at `H:/Qt`
(desktop) / `C:/Qt` (notebook); override `Qt6_DIR` in the CMake profile
if your install lives elsewhere.

## Usage

**GUI mode (default).** Double-click the executable or run it with no
arguments. The library is auto-opened at:

| OS | Path |
|---|---|
| Windows | `%LOCALAPPDATA%\xyz-profiler\library.db` |
| Linux | `~/.local/share/xyz-profiler/library.db` |

Click *Import Collection.xml…* in the toolbar to bring in your DP4
export. The wizard parses on a background thread, shows a preview
(movie count + first few titles), and only then writes to the database.

**CLI mode.** Pass a Collection.xml as a positional argument to bypass
the GUI — useful for scripted imports or smoke tests:

```powershell
xyz-profiler.exe path\to\Collection.xml `
    --db path\to\library.db `
    --images path\to\Images `
    --search "wachowski"
```

| Flag | Purpose |
|---|---|
| `--db, -b <path>` | Override the SQLite library path |
| `--library-root, -r <dir>` | Store cover paths relative to this directory |
| `--images, -i <dir>` | Resolve cover JPGs against this directory during import |
| `--search, -s <query>` | After import, run an FTS5 search and dump hits |
| `--detail, -d <id>` | Dump full detail for the movie with that ID |

## Configuration

User preferences live in an INI file under the platform's app-config
directory:

| OS | Path |
|---|---|
| Windows | `%APPDATA%\xyz-profiler\xyz-profiler.ini` |
| Linux | `~/.config/xyz-profiler/xyz-profiler.ini` |

| Key | Purpose |
|---|---|
| `tmdb/api_key` | TMDb v3 read key. Overrides the `TMDB_API_KEY` env var. |
| `library/images_directory` | Default cover-images directory used by the import wizard. |
| `ui/theme` | `Dark` / `Light` / `System` — applied via the Fusion `QPalette` in `DarkFusionStyle`. |
| `ui/view_mode` | Last-used view (`grid` / `list`). |
| `ui/table_columns`, `ui/sort_*` | List-view column visibility and sort order. |

TMDb key, images directory and theme are editable from the in-app
*Settings…* dialog; view mode, columns and sort are persisted as you use
the app.

Network responses (TMDb JSON + poster JPEGs) are cached on disk at the
platform cache location:

| OS | Path |
|---|---|
| Windows | `%LOCALAPPDATA%\xyz-profiler\cache\network` |
| Linux | `~/.cache/xyz-profiler/network` |

The TMDb client's `QNetworkDiskCache` lives here so poster thumbnails
survive restarts; downloaded full-size posters are saved into the library's
`covers/` directory and painted via an in-memory `QPixmapCache`.

## Packaging

A signed-free, single-EXE installer is produced by the Inno Setup script
in `packaging/windows/installer.iss`. Locally:

```powershell
$staged = "$pwd\build\stage\xyz-profiler"
ISCC.exe `
  "/DAppVersion=0.3.0" `
  "/DStagedRoot=$staged" `
  "/DAppIconFile=$pwd\resources\app.ico" `
  packaging\windows\installer.iss
```

The GitHub Actions workflow (`.github/workflows/build-windows.yml`) does
this end-to-end on every push: configures with Qt 6.10, builds with
Ninja + MSVC, runs `ctest`, stages with `windeployqt`, and produces both
a portable ZIP and an Inno Setup installer. Tagged `v*.*.*` pushes are
attached to a GitHub release automatically.

## Project layout

```
src/
  domain/             # header-only data model — reusable for non-film media
  importers/
    dvdprofiler/      # DP4 Collection.xml importer
  db/                 # SQLite Database / Migrations / MovieRepository
  tmdb/               # TMDb v3 async client + types
  models/             # MovieListModel / MovieTreeModel (+ sort proxies)
  controllers/        # LibraryController, SettingsController (QObject)
  ui/                 # Qt Widgets — MainWindow, cover grid, detail pane,
                      #   delegates, theming, and the dialogs
  main.cpp            # entry point — CLI + GUI dual mode
resources/            # app icon, Win32 RC, Qt resource bundle
translations/         # .ts files (de_DE)
tests/                # QtTest binaries — importer + repository
packaging/windows/    # Inno Setup script
.github/workflows/    # CI build, test, package, release
```

## Status

The original roadmap is complete and then some: DP4 import, SQLite
persistence with FTS5 search, a Qt Widgets cover grid + list/tree view +
detail pane, TMDb matching, the *Add a title* discover lookup, **bulk TMDb
matching**, **add / edit / delete** with custom cover images, user settings
with dark/light/system theming, and the two-phase async import wizard. (The
UI was migrated from the original Qt Quick/QML prototype to Qt Widgets — a
better fit for this data-heavy desktop app.) Validated against a real
369-entry library; 48 unit tests across the importer and repository.

Post-MVP items still open:

- *Refresh from TMDb* — overwrite local metadata in bulk with TMDb data
  (the edit dialog already offers per-field TMDb prefill; this is the
  one-click, whole-library variant)
- Loan tracking UI (the data is already read from DP4 — just no editor)
- Barcode scanning
- Multi-user / cross-device sync

## Origin

DVD Profiler 4 was a Windows-only collection manager that shipped with
the Invelos online metadata API. Both the app and the API are no longer
maintained. xyz-profiler reads the documented `Collection.xml` export
format and replaces the online lookup with TMDb — so existing DP4 users
can keep their catalogue on a current, maintained stack without
re-cataloguing.

## License

xyz-profiler is licensed under the **GNU General Public License v3.0 or
later** (`GPL-3.0-or-later`). See [`LICENSE`](LICENSE) for the full
text. In short: you may use, modify, and redistribute this software, but
derivative works must remain under GPL-3.0 and carry source.

### TMDB attribution

This product uses the TMDB API but is not endorsed, certified, or
otherwise approved by TMDB. Data and images fetched from TMDB at runtime
are **not** covered by this project's GPL-3.0 license — they remain
subject to the [TMDB API Terms of Use](https://www.themoviedb.org/api-terms-of-use).
Each user must obtain their own free API key from
[themoviedb.org](https://www.themoviedb.org/settings/api).
