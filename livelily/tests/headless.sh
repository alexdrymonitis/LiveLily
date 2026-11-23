#!/bin/bash
# builds the core and the headless runner with CMake (in build/, see CMakeLists.txt), and runs the tests with CTest:
# every test file with a snapshot runs with no window and no openFrameworks, and its dump is compared with the snapshot
# of the app's test mode up to the sections of the editor (see tests/compare.cmake)
#   tests/headless.sh            builds and runs every test
#   tests/headless.sh -R name    runs the tests whose names match (any options of ctest)
cd "$(dirname "$0")/.."
{ [ -f build/CMakeCache.txt ] || cmake -B build > /dev/null; } && cmake --build build -j2 > /dev/null || exit 1
ctest --test-dir build -j2 --output-on-failure "$@"
