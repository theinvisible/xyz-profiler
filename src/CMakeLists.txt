# --- Domain layer ----------------------------------------------------------
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
        domain/MediaFormat.h
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

# --- TMDb client -----------------------------------------------------------
add_library(xyz_tmdb STATIC
    tmdb/TmdbClient.h
    tmdb/TmdbClient.cpp
    tmdb/TmdbTypes.h
)
target_link_libraries(xyz_tmdb
    PUBLIC xyz_domain Qt6::Core Qt6::Network
)
target_include_directories(xyz_tmdb
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

# --- Application (Qt Widgets) ---------------------------------------------
qt_add_executable(xyz-profiler
    main.cpp
    models/MovieListModel.h
    models/MovieListModel.cpp
    models/MovieSortProxyModel.h
    models/MovieSortProxyModel.cpp
    models/MovieTableModel.h
    models/MovieTableModel.cpp
    models/MovieTreeModel.h
    models/MovieTreeModel.cpp
    controllers/LibraryController.h
    controllers/LibraryController.cpp
    controllers/SettingsController.h
    controllers/SettingsController.cpp
    ui/DarkFusionStyle.h
    ui/DarkFusionStyle.cpp
    ui/Theme.h
    ui/Theme.cpp
    ui/IconFactory.h
    ui/IconFactory.cpp
    ui/CoverArt.h
    ui/CoverArt.cpp
    ui/CoverCache.h
    ui/FlowLayout.h
    ui/FlowLayout.cpp
    ui/MovieRowDelegate.h
    ui/MovieRowDelegate.cpp
    ui/MainWindow.h
    ui/MainWindow.cpp
    ui/CoverGridWidget.h
    ui/CoverGridWidget.cpp
    ui/MovieDetailWidget.h
    ui/MovieDetailWidget.cpp
    ui/TmdbMatchDialog.h
    ui/TmdbMatchDialog.cpp
    ui/AddTitleDialog.h
    ui/AddTitleDialog.cpp
    ui/BulkTmdbMatchDialog.h
    ui/BulkTmdbMatchDialog.cpp
    ui/EditMovieDialog.h
    ui/EditMovieDialog.cpp
    ui/ImportPreviewDialog.h
    ui/ImportPreviewDialog.cpp
    ui/SettingsDialog.h
    ui/SettingsDialog.cpp
    ${CMAKE_SOURCE_DIR}/resources/app.qrc
    $<$<PLATFORM_ID:Windows>:${CMAKE_SOURCE_DIR}/resources/app.rc>
)

target_compile_definitions(xyz-profiler PRIVATE
    APP_VERSION_STRING="${PROJECT_VERSION}"
)

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
        xyz_tmdb
        Qt6::Core
        Qt6::Concurrent
        Qt6::Gui
        Qt6::Network
        Qt6::Widgets
        Qt6::Sql
        Qt6::Svg
)

target_include_directories(xyz-profiler
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/ui
)

if(WIN32)
    find_program(WINDEPLOYQT_EXE windeployqt HINTS "${CMAKE_PREFIX_PATH}/bin")
    if(WINDEPLOYQT_EXE)
        add_custom_command(TARGET xyz-profiler POST_BUILD
            COMMAND "${WINDEPLOYQT_EXE}"
                    --translations de,en
                    --no-system-d3d-compiler
                    --no-opengl-sw
                    "$<TARGET_FILE:xyz-profiler>"
            COMMENT "Deploying Qt runtime next to xyz-profiler.exe"
            VERBATIM
        )
    else()
        message(WARNING
            "windeployqt not found — xyz-profiler.exe will need Qt's bin/ on PATH.")
    endif()
endif()
