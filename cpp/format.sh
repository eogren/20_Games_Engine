#!/usr/bin/env bash
# Run clang-format over cpp/src.
#   ./cpp/format.sh           — check only, exits non-zero if anything would change
#   ./cpp/format.sh --write   — rewrite files in place
set -euo pipefail

cd "$(dirname "$0")"

if ! command -v clang-format >/dev/null 2>&1; then
    echo "clang-format not found on PATH. brew install clang-format" >&2
    exit 127
fi

mode="check"
if [[ "${1:-}" == "--write" ]]; then
    mode="write"
fi

files=()
while IFS= read -r f; do
    files+=("$f")
done < <(find src -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' -o -name '*.mm' -o -name '*.m' \) | sort)

if [[ ${#files[@]} -eq 0 ]]; then
    echo "no source files found under src/"
    exit 0
fi

if [[ "$mode" == "write" ]]; then
    clang-format -i --style=file "${files[@]}"
    echo "formatted ${#files[@]} files"
else
    clang-format --dry-run --Werror --style=file "${files[@]}"
    echo "format check passed (${#files[@]} files)"
fi
