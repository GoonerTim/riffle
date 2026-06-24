cmake_minimum_required(VERSION 3.20)
project(riffle VERSION 0.1.0 LANGUAGES CXX)

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
find_package(simdjson CONFIG REQUIRED)

# ---------------------------------------------------------------------------
# Core library — sources are added per TDD step.
# ---------------------------------------------------------------------------
add_library(riffle_core
  src/domain/version.cpp
  src/domain/types.cpp
  src/domain/factories.cpp
  src/domain/type_resolve.cpp
)
target_include_directories(riffle_core PUBLIC include)
target_link_libraries(riffle_core
  PUBLIC  Arrow::arrow_shared Parquet::parquet_shared
  PRIVATE simdjson::simdjson
)

# ---------------------------------------------------------------------------
# CLI executable (added in a later step)
# ---------------------------------------------------------------------------
# add_executable(riffle src/cli/main.cpp)
# target_link_libraries(riffle PRIVATE riffle_core)

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
)
target_link_libraries(riffle_tests
  PRIVATE riffle_core GTest::gtest GTest::gtest_main GTest::gmock
)

include(GoogleTest)
gtest_discover_tests(riffle_tests)
