#!/bin/sh
# Builds and runs the host tests. Optional: HOMETIDY_REAL_LAYOUT / HOMETIDY_REAL_ICON
# point at real console files for round trip checks.
set -eu
cd "$(dirname "$0")/.."
OUT="${TMPDIR:-/tmp}/hometidy_tests"
${CXX:-c++} -std=c++17 -Wall -Wextra -Werror -O1 -fsanitize=address,undefined \
    tests/test_main.cpp source/layout.cpp source/sorter.cpp source/titles.cpp \
    source/badge.cpp source/tasks.cpp source/fsx.cpp source/default_rules.cpp -o "$OUT"
"$OUT"
