cmake_minimum_required(VERSION 3.20)
project(dod_core VERSION 0.1.0 LANGUAGES CXX)

# ── Dependencies ──────────────────────────────────────────────
include(FetchContent)

FetchContent_Declare(
    EnTT
    GIT_REPOSITORY https://github.com/skypjack/entt.git
    GIT_TAG        v3.14.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(EnTT)

# ── Library ───────────────────────────────────────────────────
add_library(dod_core STATIC
    src/world.cpp
    src/system_graph.cpp
    src/thread_pool.cpp
    src/scheduler.cpp
)
add_library(dod::dod_core ALIAS dod_core)

target_compile_features(dod_core PUBLIC cxx_std_20)

target_include_directories(dod_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(dod_core PUBLIC EnTT::EnTT)

target_compile_options(dod_core PUBLIC -fno-rtti)

target_compile_options(dod_core PRIVATE
    -Wall -Wextra -Wpedantic -Werror
    -Wno-unused-parameter
    -Wno-deprecated-literal-operator
)

# ── Tests ─────────────────────────────────────────────────────
include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

# ── Install ───────────────────────────────────────────────────
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS dod_core
    EXPORT dod_core-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY include/dod_core
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT dod_core-targets
    NAMESPACE dod::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dod_core
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/dod_core-config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/dod_core-config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dod_core
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/dod_core-config-version.cmake"
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/dod_core-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/dod_core-config-version.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dod_core
)
