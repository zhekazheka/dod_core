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

add_executable(test_query test_query.cpp)
target_link_libraries(test_query PRIVATE dod_core GTest::gtest_main)

add_executable(test_system_traits test_system_traits.cpp)
target_link_libraries(test_system_traits PRIVATE dod_core GTest::gtest_main)

add_executable(test_system test_system.cpp)
target_link_libraries(test_system PRIVATE dod_core GTest::gtest_main)

add_executable(test_system_graph test_system_graph.cpp)
target_link_libraries(test_system_graph PRIVATE dod_core GTest::gtest_main)

add_executable(test_thread_pool test_thread_pool.cpp)
target_link_libraries(test_thread_pool PRIVATE dod_core GTest::gtest_main)

add_executable(test_scheduler test_scheduler.cpp)
target_link_libraries(test_scheduler PRIVATE dod_core GTest::gtest_main)

add_executable(test_phase test_phase.cpp)
target_link_libraries(test_phase PRIVATE dod_core GTest::gtest_main)

add_executable(test_integration test_integration.cpp)
target_link_libraries(test_integration PRIVATE dod_core GTest::gtest_main)

# ── Register with CTest ──────────────────────────────────────
include(GoogleTest)
gtest_discover_tests(test_entity)
gtest_discover_tests(test_world)
gtest_discover_tests(test_query)
gtest_discover_tests(test_system_traits)
gtest_discover_tests(test_system)
gtest_discover_tests(test_system_graph)
gtest_discover_tests(test_thread_pool)
gtest_discover_tests(test_scheduler)
gtest_discover_tests(test_phase)
gtest_discover_tests(test_integration)
