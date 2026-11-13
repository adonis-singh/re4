#!/bin/sh
# Builds and runs the host unit test for the Xbox controls layer (src/xbox/pad_xbox.cpp).
# Needs only a C and a C++ compiler; no Xbox toolchain, SDL or game assets.
#   usage: tools/xbox/run_pad_test.sh        (CC / CXX override the compilers)
set -eu

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
OUT=${TMPDIR:-/tmp}/re4_pad_test.$$
mkdir -p "$OUT"
trap 'rm -rf "$OUT"' EXIT
CC=${CC:-cc}
CXX=${CXX:-c++}

# The original Dolphin clamp, with its entry points renamed so both versions can live in one binary.
"$CC" -c -w -I "$ROOT/tools/xbox/stubs" -DPADClamp=PADClamp_orig -DPADClampCircle=PADClampCircle_orig \
    "$ROOT/src/lib/Padclamp.c" -o "$OUT/padclamp_orig.o"

# The layer under test, with the Xbox define on, and the fake backend in the test itself.
"$CXX" -std=gnu++11 -Wall -Wextra -DRE4_XBOX -include "$ROOT/tools/xbox/host_compat.h" \
    -I "$ROOT/include" \
    "$ROOT/tools/xbox/pad_test.cpp" "$ROOT/src/xbox/pad_xbox.cpp" "$OUT/padclamp_orig.o" \
    -lm -o "$OUT/pad_test"

"$OUT/pad_test"
