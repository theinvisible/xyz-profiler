# --- Domain layer ----------------------------------------------------------
# Pure data structs, no I/O. Header-only so it stays usable from any
# downstream module (importers, persistence, models, UI) without forcing a
# specific build configuration. Reusable for future non-film media (games).
add_library(xyz_domain INTERFACE)

target_sources(xyz_domain INTERFACE
    FILE_SET HEADERS
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
    FILES
        domain/AudioTrack.h
        domain/BoxSet.h
        domain/CollectionMembership.h
        domain/CustomField.h
        domain/Disc.h
        domain/Event.h
        domain/IdMetadata.h
        domain/LoanInfo.h
        domain/MediaItem.h
        domain/MonetaryAmount.h
        domain/Movie.h
        domain/Person.h
        domain/PurchaseInfo.h
        domain/RatingInfo.h
        domain/Review.h
        domain/VideoFormat.h
)

target_include_directories(xyz_domain INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(xyz_domain INTERFACE
    Qt6::Core
)

# --- Importers -------------------------------------------------------------
add_library(xyz_importers STATIC
    importers/Importer.h
    importers/dvdprofiler/DvdProfilerXmlImporter.h
    importers/dvdprofiler/DvdProfilerXmlImporter.cpp
)
target_link_libraries(xyz_importers
    PUBLIC xyz_domain Qt6::Core
)
target_include_directories(xyz_importers
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

# --- Persistence -----------------------------------------------------------
add_library(xyz_db STATIC
    db/Database.h
    db/Database.cpp
    db/Migrations.h
    db/Migrations.cpp
    db/MovieRepository.h
    db/MovieRepository.cpp
)
target_link_libraries(xyz_db
    PUBLIC xyz_domain Qt6::Core Qt6::Sql
)
target_include_directories(xyz_db
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

# --- Application + QML module --------------------------------------------
# Executable first; the QML module then attaches to it. The QML URI
# `xyz.profiler` is what view files use (`import xyz.profiler`). C++ types
# (MovieListModel, LibraryController) are annotated with
# QML_ELEMENT / QML_SINGLETON and picked up by qt_add_qml_module().
qt_add_executable(xyz-profiler
    main.cpp
    ${CMAKE_SOURCE_DIR}/resources/app.qrc
    $<$<PLATFORM_ID:Windows>:${CMAKE_SOURCE_DIR}/resources/app.rc>
)

qt_add_qml_module(xyz-profiler
    URI xyz.profiler
    VERSION 1.0
    SOURCES
        models/MovieListModel.h
        models/MovieListModel.cpp
        controllers/LibraryController.h
        controllers/LibraryController.cpp
    QML_FILES
        qml/Main.qml
        qml/CoverGrid.qml
        qml/MovieDetail.qml
    # Qt's QML engine auto-searches `:/qt/qml/` on the resource system;
    # putting the module there lets `loadFromModule("xyz.profiler", "Main")`
    # work without an extra `engine.addImportPath()` call in main.
    RESOURCE_PREFIX /qt/qml
)

# Expose project version to C++ so the about dialog / window title can
# show it, and so the GitHub workflow can extract it from CMakeLists.txt
# via regex.
target_compile_definitions(xyz-profiler PRIVATE
    APP_VERSION_STRING="${PROJECT_VERSION}"
)

# ---- Translations --------------------------------------------------------
# .ts → .qm at build time, embedded under :/i18n. main.cpp picks the best
# match for QLocale::system() and silently falls back to the source
# language (English) when no .qm matches.
set(TS_FILES
    ${CMAKE_SOURCE_DIR}/translations/xyz-profiler_de.ts
)
qt_add_translations(xyz-profiler
    TS_FILES ${TS_FILES}
    RESOURCE_PREFIX "/i18n"
)

target_link_libraries(xyz-profiler
    PRIVATE
        xyz_domain
        xyz_importers
        xyz_db
        Qt6::Core
        Qt6::Concurrent
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Sql
)

target_include_directories(xyz-profiler
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        # qt_add_qml_module's generated type-registrations file uses
        # `<HeaderName.h>` (bare filename) and relies on these dirs being
        # on the include path.
        ${CMAKE_CURRENT_SOURCE_DIR}/controllers
        ${CMAKE_CURRENT_SOURCE_DIR}/models
)

# --- Deploy Qt runtime DLLs next to the executable on Windows -------------
# Without this, launching the exe from anywhere other than a shell that
# has Qt's bin/ on PATH fails with STATUS_DLL_NOT_FOUND (0xC0000135).
# `windeployqt` copies the needed Qt6 DLLs, QML modules, and platform
# plugins into the output directory; it's a no-op on subsequent builds
# when nothing relevant changed.
if(WIN32)
    find_program(WINDEPLOYQT_EXE windeployqt HINTS "${CMAKE_PREFIX_PATH}/bin")
    if(WINDEPLOYQT_EXE)
        add_custom_command(TARGET xyz-profiler POST_BUILD
            COMMAND "${WINDEPLOYQT_EXE}"
                    --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/qml"
                    --no-translations
                    --no-system-d3d-compiler
                    --no-opengl-sw
                    "$<TARGET_FILE:xyz-profiler>"
            COMMENT "Deploying Qt runtime next to xyz-profiler.exe"
            VERBATIM
        )
    else()
        message(WARNING
            "windeployqt not found — xyz-profiler.exe will need Qt's bin/ "
            "on PATH at launch time. Expected under \${CMAKE_PREFIX_PATH}/bin.")
    endif()
endif()
