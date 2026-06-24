# Contributing to Riffle

**🇬🇧 English** · [🇷🇺 Русский](CONTRIBUTING.ru.md)

Thanks for your interest in Riffle! This guide explains how to propose changes and what we
expect from contributions.

## Code of conduct

Be respectful and constructive. We follow the spirit of the
[Contributor Covenant](https://www.contributor-covenant.org/). Harassment of any kind is not
tolerated.

## Before you start

1. **Read the spec.** [`docs/riffle.md`](docs/riffle.md) is the source of truth for behaviour,
   data types, the state machine, and the public contract. Changes should stay consistent with
   it — or update it in the same pull request.
2. **Open an issue first** for anything non-trivial (new features, API changes, dependencies).
   This avoids duplicated work and lets us agree on the approach early.
3. Good first contributions: tests, documentation, small bug fixes, benchmarks.

## Development setup

```bash
# Dependencies (Debian/Ubuntu). Arrow/Parquet come from the Apache Arrow APT repo:
sudo apt-get update
sudo apt-get install -y -V ca-certificates lsb-release wget
wget https://apache.jfrog.io/artifactory/arrow/$(lsb_release --id --short | tr 'A-Z' 'a-z')/apache-arrow-apt-source-latest-$(lsb_release --codename --short).deb
sudo apt-get install -y -V ./apache-arrow-apt-source-latest-$(lsb_release --codename --short).deb
sudo apt-get update
sudo apt-get install -y -V build-essential cmake ninja-build \
    libarrow-dev libparquet-dev libzstd-dev libsnappy-dev \
    libsimdjson-dev libgtest-dev

# Build & test via the task runner
just build
just test
```

We use [`just`](https://github.com/casey/just) as a task runner — run `just` to list tasks.

## Coding guidelines

- **Language:** C++23 (uses `std::expected`).
- **Style: functional core, imperative shell.** Prefer free functions over orchestrator
  classes; model data as immutable value types constructed by `make_*` factories. Mutability is
  allowed **only** on the hot path (read buffers, column accumulators) and must be isolated.
  When style conflicts with throughput on the hot path, **performance wins**.
- **No magic numbers.** Every tunable lives as a named constant (see the *Constants* section in
  the spec).
- **Errors:** recoverable failures return `std::expected<T, std::string>`; invariant violations
  in `make_*` factories throw `std::invalid_argument`.
- **Formatting & linting:** run `just fmt` (clang-format) and `just lint` (clang-tidy) before
  pushing. CI enforces both.

## Tests

- Every new function or behaviour needs tests. Follow test-driven development where practical:
  write a failing test first, then the implementation.
- Keep tests deterministic and independent of external services.
- Run the full suite locally with `just test` before opening a pull request.

## Commit messages

- Use clear, imperative subject lines: `Add zstd codec option`, not `added stuff`.
- Reference issues where relevant: `Fix line splitting at buffer boundary (#42)`.
- Keep each commit focused on a single logical change.

## Pull requests

1. Fork the repo and create a topic branch off `main`.
2. Make your change, with tests and updated docs/spec as needed.
3. Ensure `just build`, `just test`, `just fmt` and `just lint` all pass — CI runs the same
   checks on every push and pull request (see
   [`.github/workflows/ci.yml`](.github/workflows/ci.yml)).
4. Open a pull request describing **what** changed and **why**. Link the related issue.
5. A maintainer will review; please respond to feedback and keep the branch up to date.

By contributing, you agree that your contributions are licensed under the
[MIT License](LICENSE).
