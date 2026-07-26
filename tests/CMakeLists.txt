# --- Helper for registering QtTest binaries --------------------------------
# Keeps each test self-contained: own exe, own ctest registration, and on
# Windows PATH-injection so the loader can find Qt's DLLs.
function(xyz_add_test name source)
    qt_add_executable(${name} ${source})

    target_link_libraries(${name} PRIVATE Qt6::Test ${ARGN})
    target_compile_definitions(${name} PRIVATE
        TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
    )

    add_test(NAME ${name} COMMAND ${name})

    if(WIN32)
        get_target_property(_qt_core_loc Qt6::Core LOCATION)
        get_filename_component(_qt_bin_dir "${_qt_core_loc}" DIRECTORY)
        set_tests_properties(${name} PROPERTIES
            ENVIRONMENT "PATH=${_qt_bin_dir};$ENV{PATH}"
        )
    endif()
endfunction()

# --- Test binaries ---------------------------------------------------------
xyz_add_test(test_dvdprofiler_xml_importer
    test_dvdprofiler_xml_importer.cpp
    xyz_importers
)

xyz_add_test(test_domain_ops
    test_domain_ops.cpp
    xyz_domain
)

xyz_add_test(test_movie_repository
    test_movie_repository.cpp
    xyz_db
    Qt6::Sql
)

# CalendarBuckets is the only non-UI calendar logic; compile its .cpp into the
# test directly (the helper takes a single source) and expose src/ for includes.
xyz_add_test(test_calendar_buckets
    test_calendar_buckets.cpp
    xyz_domain
)
target_sources(test_calendar_buckets PRIVATE
    ${CMAKE_SOURCE_DIR}/src/ui/CalendarBuckets.cpp)
target_include_directories(test_calendar_buckets PRIVATE ${CMAKE_SOURCE_DIR}/src)

# The collection filter and the list model live in the executable target, not
# in a library — compile them into the test directly (same approach as
# CalendarBuckets above). Both are Qt6::Core only, so this stays guiless.
xyz_add_test(test_collection_filter
    test_collection_filter.cpp
    xyz_domain
)
target_sources(test_collection_filter PRIVATE
    ${CMAKE_SOURCE_DIR}/src/models/CollectionFilterProxyModel.cpp
    ${CMAKE_SOURCE_DIR}/src/models/MovieListModel.cpp)
target_include_directories(test_collection_filter PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/models)
