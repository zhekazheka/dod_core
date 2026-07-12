set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(BENCHMARK_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_WERROR OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG        v1.9.4
    GIT_SHALLOW    TRUE
    SYSTEM
    FIND_PACKAGE_ARGS
)
# benchmark's own build uses -pedantic-errors, which newer clang trips on:
# its __COUNTER__ feature probe is a "C2y extension". Directory-scoped, so it
# only affects the benchmark subtree and the benchmark executables below.
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-c2y-extensions")
endif()

FetchContent_MakeAvailable(benchmark)

add_executable(dod_benchmarks
    bench_scheduler.cpp
    bench_view.cpp
    bench_parallel.cpp
)
target_link_libraries(dod_benchmarks PRIVATE dod_core benchmark::benchmark_main)
