cmake_minimum_required(VERSION 3.25)
project(riffle VERSION 0.1.3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------
find_package(Arrow CONFIG REQUIRED)
find_package(Parquet CONFIG REQUIRED)

# simdjson is pinned via FetchContent so the exact header is used everywhere
# (build and clang-tidy), independent of the distro's packaged version.
include(FetchContent)
set(SIMDJSON_TAG "v3.11.0" CACHE STRING "simdjson git tag")
FetchContent_Declare(
  simdjson
  GIT_REPOSITORY https://github.com/simdjson/simdjson.git
  GIT_TAG ${SIMDJSON_TAG}
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(simdjson)
# Treat simdjson's headers as system headers so clang-tidy does not analyze them.
if(TARGET simdjson)
  set_target_properties(simdjson PROPERTIES SYSTEM TRUE)
endif()

# ---------------------------------------------------------------------------
# Core library — sources are added per TDD step.
# ---------------------------------------------------------------------------
add_library(riffle_core
  src/domain/version.cpp
  src/domain/types.cpp
  src/domain/factories.cpp
  src/domain/type_resolve.cpp
  src/domain/schema.cpp
  src/domain/timestamp.cpp
  src/adapters/batch.cpp
  src/adapters/parquet_writer.cpp
  src/adapters/writer.cpp
  src/adapters/columnar_raw_writer.cpp
  src/adapters/reader.cpp
  src/adapters/json_parser.cpp
  src/adapters/schema_json.cpp
  src/app/convert.cpp
  src/app/stats.cpp
  src/cli/args.cpp
)
target_include_directories(riffle_core PUBLIC include)
target_link_libraries(riffle_core
  PUBLIC  Arrow::arrow_shared Parquet::parquet_shared
  PRIVATE simdjson::simdjson
)

# ---------------------------------------------------------------------------
# CLI executable
# ---------------------------------------------------------------------------
add_executable(riffle src/cli/main.cpp)
target_link_libraries(riffle PRIVATE riffle_core)

# ---------------------------------------------------------------------------
# Stage-ablation profiler (optional: -DRIFFLE_BUILD_PROFILER=ON)
# ---------------------------------------------------------------------------
option(RIFFLE_BUILD_PROFILER "Build the stage-ablation profiler" OFF)
if(RIFFLE_BUILD_PROFILER)
  add_executable(riffle_profile bench/profile_stages.cpp)
  target_link_libraries(riffle_profile PRIVATE riffle_core simdjson::simdjson)
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
enable_testing()
find_package(GTest CONFIG REQUIRED)

add_executable(riffle_tests
  tests/domain/version_test.cpp
  tests/domain/types_test.cpp
  tests/domain/factories_test.cpp
  tests/domain/type_resolve_test.cpp
  tests/domain/infer_schema_test.cpp
  tests/domain/merge_override_test.cpp
  tests/domain/projection_test.cpp
  tests/domain/timestamp_test.cpp
  tests/domain/schema_json_test.cpp
  tests/adapters/batch_test.cpp
  tests/adapters/parquet_writer_test.cpp
  tests/adapters/columnar_raw_writer_test.cpp
  tests/adapters/reader_test.cpp
  tests/adapters/json_parser_test.cpp
  tests/app/convert_test.cpp
  tests/cli/args_test.cpp
)
target_link_libraries(riffle_tests
  PRIVATE riffle_core GTest::gtest GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(riffle_tests)
