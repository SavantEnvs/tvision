#!/usr/bin/env bash
# mayhem/build.sh — build tvision (sanitized) + the libFuzzer harnesses in mayhem/harnesses, and
# upstream's gtest suite (normal flags) for mayhem/test.sh. Re-runnable and offline (deps are apt).
set -euo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
export CC CXX
# Two UBSan checks are relaxed (ASan and every other UBSan check stay on and halting):
#  - pointer-overflow: TEditor::insertBuffer rebases a pointer into its own buffer with
#    `p -= ptrdiff_t(buffer)` (teditor2.cpp), which every auto-indented newline reaches, so it would
#    halt nearly every editor input.
#  - object-size: ncursinp.cpp's static key table is built through const_unordered_map::
#    with_string_keys, whose static_cast of init.end() trips the check during static
#    initialization, i.e. before any input, in every binary that links the platform layer.
#    (ASan still catches real out-of-bounds accesses.)
SANITIZER_FLAGS="$SANITIZER_FLAGS${SANITIZER_FLAGS:+ -fno-sanitize=pointer-overflow,object-size}"
cd "$SRC"

# 1) The library, instrumented (no build type: keep upstream's asserts; no PCH so flags stay uniform).
cmake -S . -B build-fuzz -DTV_BUILD_EXAMPLES=OFF -DTV_BUILD_TESTS=OFF -DTV_BUILD_USING_GPM=OFF \
      -DTV_OPTIMIZE_BUILD=OFF -DCMAKE_BUILD_TYPE= \
      -DCMAKE_C_FLAGS="-O1 $SANITIZER_FLAGS -fsanitize=fuzzer-no-link $DEBUG_FLAGS" \
      -DCMAKE_CXX_FLAGS="-O1 $SANITIZER_FLAGS -fsanitize=fuzzer-no-link $DEBUG_FLAGS"
cmake --build build-fuzz -j"$MAYHEM_JOBS" --target tvision

# 2) Harnesses: a libFuzzer binary and a run-once -standalone reproducer each.
mkdir -p build-fuzz/mayhem
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o build-fuzz/mayhem/standalone_main.o
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c mayhem/lsan_off.cc -o build-fuzz/mayhem/lsan_off.o
INC=(-I"$SRC/include" -I"$SRC/include/tvision")
LIBS=(build-fuzz/libtvision.a -lncursesw -ltinfo -lpthread)
for src in mayhem/harnesses/fuzz_*.cpp; do
    name=$(basename "$src" .cpp)
    $CXX -std=c++14 -O1 $SANITIZER_FLAGS $DEBUG_FLAGS -fsanitize=fuzzer-no-link "${INC[@]}" \
        -c "$src" -o "build-fuzz/mayhem/$name.o"
    $CXX $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "build-fuzz/mayhem/$name.o" \
        build-fuzz/mayhem/lsan_off.o "${LIBS[@]}" -o "/mayhem/$name"
    $CXX $SANITIZER_FLAGS $DEBUG_FLAGS build-fuzz/mayhem/standalone_main.o "build-fuzz/mayhem/$name.o" \
        build-fuzz/mayhem/lsan_off.o "${LIBS[@]}" -o "/mayhem/$name-standalone"
done

# 3) Upstream's gtest suite + the tvhc help compiler, with normal flags (the functional oracle).
#    TV_BUILD_TESTS also runs the suite once at build time; test.sh runs it again and reports counts.
cmake -S . -B build-tests -DTV_BUILD_EXAMPLES=ON -DTV_BUILD_TESTS=ON -DTV_BUILD_USING_GPM=OFF \
      -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DCMAKE_C_FLAGS="$COVERAGE_FLAGS" -DCMAKE_CXX_FLAGS="$COVERAGE_FLAGS"
cmake --build build-tests -j"$MAYHEM_JOBS" --target tvision-test tvhc
