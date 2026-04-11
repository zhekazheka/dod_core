FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
    GIT_SHALLOW    TRUE
)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# ── Test executables ──────────────────────────────────────────
add_executable(test_entity test_entity.cpp)
target_link_libraries(test_entity PRIVATE dod_core GTest::gtest_main)

add_executable(test_world test_world.cpp)
target_link_libraries(test_world PRIVATE dod_core GTest::gtest_main)

# ── Register with CTest ──────────────────────────────────────
include(GoogleTest)
gtest_discover_tests(test_entity)
gtest_discover_tests(test_world)
