# Repository Guidelines

## Project Structure & Module Organization

`src/` contains the C++20/Qt application. Domain structs live in `src/domain`, import logic in `src/importers`, SQLite persistence in `src/db`, TMDb networking in `src/tmdb`, models in `src/models`, controllers in `src/controllers`, and Qt Widgets UI in `src/ui`. `resources/` holds Qt resources, icons, and Windows resource files. `translations/` stores Qt `.ts` translation sources. `tests/` contains QtTest binaries and fixture data under `tests/data`. Packaging scripts live in `packaging/`.

## Build, Test, and Development Commands

Configure a build directory:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
```

If Qt is outside `H:/Qt` or `C:/Qt`, add `-DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/msvc2022_64"`.

Build the app:

```powershell
cmake --build build --target xyz-profiler -j
```

Run tests:

```powershell
ctest --test-dir build --output-on-failure
```

On Windows, `windeployqt` runs after building when available, so the generated executable can find Qt runtime files.

## Coding Style & Naming Conventions

Use C++20 and Qt idioms already present in the codebase. Prefer `QStringLiteral` for fixed strings, Qt containers and value types where the surrounding code uses them, and signal/slot patterns for UI behavior. Keep namespaces explicit for application code (`xyz::`). Follow the existing 4-space indentation, brace placement, and grouped include style: local headers first, then Qt/system headers. Name classes in `PascalCase`, methods and variables in `camelCase`, and constants or roles descriptively.

## Testing Guidelines

Tests use QtTest and are registered through `xyz_add_test` in `tests/CMakeLists.txt`. Add new tests as focused `test_*.cpp` files, link only the modules under test, and place reusable fixtures in `tests/data`. Prefer deterministic tests with temporary directories or in-memory/local SQLite databases. Run `ctest --test-dir build --output-on-failure` before submitting changes.

## Commit & Pull Request Guidelines

Recent commits use imperative, descriptive subjects such as `Introduce BulkTmdbMatchDialog` or `Align grid and list view sorting`. Keep the first line concise and explain behavior, not implementation trivia. Pull requests should include a short summary, test results, related issues, and screenshots or screen recordings for visible UI changes. Mention configuration impacts, migrations, or packaging changes explicitly.

## Security & Configuration Tips

Do not commit TMDb API keys, user databases, or generated build directories. Runtime settings belong in the platform config file or environment variables such as `TMDB_API_KEY`. Keep local build output in ignored directories like `build/` or `cmake-build-debug/`.
