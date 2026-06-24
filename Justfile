# Riffle — task runner (https://github.com/casey/just)
# Run `just` with no arguments to list available recipes.

# Build configuration
build_type := "Release"
build_dir  := "build"
jobs       := num_cpus()

# Default: list recipes
default:
    @just --list

# Configure the CMake build (Arrow/Parquet/simdjson/GTest come from the system).
configure:
    cmake -S . -B {{build_dir}} -DCMAKE_BUILD_TYPE={{build_type}} -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Configure (if needed) and build
build: configure
    cmake --build {{build_dir}} -j {{jobs}}

# Run the test suite
test: build
    ctest --test-dir {{build_dir}} --output-on-failure

# Format all sources in place (clang-format)
fmt:
    git ls-files '*.cpp' '*.hpp' '*.h' | xargs --no-run-if-empty clang-format -i

# Check formatting without modifying files (used by CI)
fmt-check:
    git ls-files '*.cpp' '*.hpp' '*.h' | xargs --no-run-if-empty clang-format --dry-run --Werror

# Static analysis (clang-tidy 19); requires a configured build for compile_commands.json.
# The two simdjson-including adapters are skipped: simdjson's amalgamated header
# does not parse under clang on some distro versions.
lint: configure
    git ls-files '*.cpp' '*.hpp' | grep -vE 'src/adapters/(json_parser|schema_json)\.cpp' | xargs --no-run-if-empty clang-tidy -p {{build_dir}}

# Build a release binary and run it against a sample
run *ARGS: build
    {{build_dir}}/riffle {{ARGS}}

# Stage-ablation profiler: where does conversion time go (read/parse/append)?
profile FILE:
    cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DRIFFLE_BUILD_PROFILER=ON
    cmake --build build-release -j {{jobs}}
    build-release/riffle_profile {{FILE}}

# Reproduce the benchmark: generate data, run all tools, render charts.
# Requires a release build in build-release/ and: pip install duckdb pyarrow pandas psutil matplotlib
bench:
    cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
    cmake --build build-release -j {{jobs}}
    python bench/gen_data.py 1000000 bench/data/bench.jsonl
    python bench/gen_data.py 3000000 bench/data/bench3m.jsonl
    python bench/bench.py
    python bench/plot.py

# Remove build artifacts
clean:
    rm -rf {{build_dir}}
