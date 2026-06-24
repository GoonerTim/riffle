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

# Static analysis (clang-tidy); requires a configured build for compile_commands.json
lint: configure
    git ls-files '*.cpp' '*.hpp' | xargs --no-run-if-empty clang-tidy -p {{build_dir}}

# Build a release binary and run it against a sample
run *ARGS: build
    {{build_dir}}/riffle {{ARGS}}

# Remove build artifacts
clean:
    rm -rf {{build_dir}}
