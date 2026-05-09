#!/usr/bin/env bash
# Run clang-tidy over cpp/src using a Ninja side-build for compile_commands.json.
# Xcode (cpp/build.sh) doesn't emit compile_commands.json, so we keep a
# separate `build/lint/` configuration purely for tidy.
#
#   ./cpp/lint.sh           — diagnose only
#   ./cpp/lint.sh --fix     — apply clang-tidy's suggested fixes
set -euo pipefail

cd "$(dirname "$0")"

if ! command -v clang-tidy >/dev/null 2>&1; then
    echo "clang-tidy not found on PATH. brew install llvm" >&2
    exit 127
fi
if ! command -v ninja >/dev/null 2>&1; then
    echo "ninja not found on PATH. brew install ninja" >&2
    exit 127
fi

LINT_DIR="build/lint"
if [[ ! -f "$LINT_DIR/compile_commands.json" ]]; then
    echo "configuring $LINT_DIR (one-time)..."
    cmake -G Ninja -B "$LINT_DIR" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON >/dev/null
fi

files=()
while IFS= read -r f; do
    files+=("$f")
done < <(find src -type f \( -name '*.cpp' -o -name '*.mm' \) | sort)

if [[ ${#files[@]} -eq 0 ]]; then
    echo "no source files found under src/"
    exit 0
fi

tidy_args=(-p "$LINT_DIR" --quiet)
if [[ "${1:-}" == "--fix" ]]; then
    tidy_args+=(--fix --fix-errors)
fi

# Filter out clang's "N warnings generated." banner — those counts are
# diagnostics fired in system/SDK headers that HeaderFilterRegex already
# excludes from being reported. The banner is a clang frontend message that
# `--quiet` does not suppress. Real diagnostics on user code still print.
# `|| true` keeps grep from returning 1 when nothing prints (clean lint);
# PIPESTATUS preserves clang-tidy's actual exit code under `set -e`.
clang-tidy "${tidy_args[@]}" "${files[@]}" 2>&1 \
    | { grep -Ev '^[0-9]+ warnings generated\.$' || true; }
exit "${PIPESTATUS[0]}"
