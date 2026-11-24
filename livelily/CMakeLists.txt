# LiveLily's core (the engine and the editor, with no openFrameworks) and its headless runner, which runs the tests with no window
# (see doc/stage3_inventory.md); the app itself is still built with openFrameworks' makefiles (make wpyo) until stage 4
#   cmake -B build && cmake --build build && ctest --test-dir build
# the headless runner is built into bin/, from where it finds LiveLily's resources in ../resources (see src/paths.h)

cmake_minimum_required(VERSION 3.20)
project(LiveLily LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE Release)
endif()

option(LIVELILY_PYO "Build with Python and pyo (needs Python's development files, and pyo installed in that Python)" ON)
set(LIVELILY_RTMIDI_DIR "" CACHE PATH "A directory with RtMidi.cpp and RtMidi.h to build with, instead of fetching RtMidi")

# FluidSynth, for SoundFonts (\instname.sndfont): with pkg-config (Linux, macOS), or as a CMake package (e.g. vcpkg on Windows)
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
	pkg_check_modules(FLUIDSYNTH IMPORTED_TARGET fluidsynth)
endif()
if(TARGET PkgConfig::FLUIDSYNTH)
	set(fluidsynth_target PkgConfig::FLUIDSYNTH)
else()
	find_package(FluidSynth CONFIG REQUIRED)
	set(fluidsynth_target FluidSynth::libfluidsynth)
endif()
# the directory of fluidsynth.h, which neither way always gives: Homebrew's pkg-config leaves out the include directory of its prefix
# as a system one (which Apple's compiler doesn't search), and FluidSynth's CMake package doesn't always give it either
# so it is looked for too, from pkg-config's includedir and next to the package (lib/cmake/fluidsynth -> include)
find_path(LIVELILY_FLUIDSYNTH_INCLUDE_DIR fluidsynth.h HINTS ${FLUIDSYNTH_INCLUDEDIR} ${FLUIDSYNTH_INCLUDE_DIRS} "${FluidSynth_DIR}/../../../include")
add_library(livelily_fluidsynth INTERFACE)
target_link_libraries(livelily_fluidsynth INTERFACE ${fluidsynth_target})
if(NOT LIVELILY_FLUIDSYNTH_INCLUDE_DIR)
	message(FATAL_ERROR "FluidSynth was found (${fluidsynth_target}), but not fluidsynth.h: give its directory with -DLIVELILY_FLUIDSYNTH_INCLUDE_DIR=...")
endif()
message(STATUS "FluidSynth: ${fluidsynth_target}, with fluidsynth.h in ${LIVELILY_FLUIDSYNTH_INCLUDE_DIR}")
target_include_directories(livelily_fluidsynth INTERFACE "${LIVELILY_FLUIDSYNTH_INCLUDE_DIR}")
set(LIVELILY_FLUIDSYNTH livelily_fluidsynth)

# Python, embedded to run pyo
if(LIVELILY_PYO)
	find_package(Python3 COMPONENTS Interpreter Development.Embed REQUIRED)
endif()

# RtMidi, for the MIDI ports of the headless runner: from a directory given, or fetched at a fixed version
if(LIVELILY_RTMIDI_DIR)
	add_library(rtmidi STATIC "${LIVELILY_RTMIDI_DIR}/RtMidi.cpp")
	target_include_directories(rtmidi PUBLIC "${LIVELILY_RTMIDI_DIR}" "${PROJECT_SOURCE_DIR}/headless/compat")
	if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
		find_package(ALSA REQUIRED)
		target_compile_definitions(rtmidi PUBLIC __LINUX_ALSA__)
		target_link_libraries(rtmidi PUBLIC ALSA::ALSA)
	elseif(APPLE)
		target_compile_definitions(rtmidi PUBLIC __MACOSX_CORE__)
		target_link_libraries(rtmidi PUBLIC "-framework CoreMIDI" "-framework CoreAudio" "-framework CoreFoundation")
	elseif(WIN32)
		target_compile_definitions(rtmidi PUBLIC __WINDOWS_MM__)
		target_link_libraries(rtmidi PUBLIC winmm)
	endif()
else()
	include(FetchContent)
	set(RTMIDI_BUILD_TESTING OFF CACHE BOOL "" FORCE)
	set(RTMIDI_BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
	set(RTMIDI_API_JACK OFF CACHE BOOL "" FORCE)
	FetchContent_Declare(rtmidi GIT_REPOSITORY https://github.com/thestk/rtmidi.git GIT_TAG 6.0.0 GIT_SHALLOW TRUE)
	FetchContent_MakeAvailable(rtmidi)
endif()

# the core: the engine (session, parser, sequencer, instruments), the editor, its grids, and the keys
set(LIVELILY_CORE_SOURCES
	src/cellgrid.cpp src/clock.cpp src/codetext.cpp src/editor.cpp src/function.cpp src/instrument.cpp src/keys.cpp src/osc.cpp
	src/parser.cpp src/paths.cpp src/sequencer.cpp src/session.cpp src/session_dump.cpp src/session_setup.cpp src/soundfont.cpp
	src/system_command.cpp src/test_midi.cpp)
if(LIVELILY_PYO)
	list(APPEND LIVELILY_CORE_SOURCES src/PyoClass.cpp src/pyo_classes.cpp)
endif()
add_library(livelily_core STATIC ${LIVELILY_CORE_SOURCES})
target_include_directories(livelily_core PUBLIC src)
find_package(Threads REQUIRED)
target_link_libraries(livelily_core PUBLIC ${LIVELILY_FLUIDSYNTH} Threads::Threads)
if(LIVELILY_PYO)
	target_compile_definitions(livelily_core PUBLIC USEPYO)
	target_link_libraries(livelily_core PUBLIC Python3::Python)
endif()
if(WIN32)
	# the Documents folder (SHGetKnownFolderPath(), see paths.cpp)
	target_link_libraries(livelily_core PUBLIC shell32 ole32)
endif()
if(MSVC)
	target_compile_options(livelily_core PRIVATE /W3 /utf-8)
	target_compile_definitions(livelily_core PUBLIC _USE_MATH_DEFINES NOMINMAX _CRT_SECURE_NO_WARNINGS)
else()
	target_compile_options(livelily_core PRIVATE -Wall -Werror=return-type)
endif()

# the headless runner
add_executable(livelily_headless headless/main.cpp headless/platform.cpp)
target_link_libraries(livelily_headless PRIVATE livelily_core rtmidi)
set_target_properties(livelily_headless PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")
foreach(config ${CMAKE_CONFIGURATION_TYPES})
	string(TOUPPER ${config} CONFIG)
	set_target_properties(livelily_headless PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${CONFIG} "${PROJECT_SOURCE_DIR}/bin")
endforeach()

# the tests: every test file (tests/lyv/*.lyv) with a snapshot in tests/snapshots runs in the headless runner
# (the test files are kept out of the published repository, so there the tests are only built and none runs)
# and its dump is compared with the snapshot up to the sections of the editor (see tests/compare.cmake)
# the snapshots are made by the app's test mode (tests/snapshots.sh save), which also checks the editor's sections and the screen
enable_testing()
file(GLOB LIVELILY_TEST_FILES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/tests/lyv/*.lyv")
foreach(test_file ${LIVELILY_TEST_FILES})
	get_filename_component(test_name "${test_file}" NAME_WE)
	set(snapshot "${PROJECT_SOURCE_DIR}/tests/snapshots/${test_name}.dump")
	if(NOT EXISTS "${snapshot}")
		continue()
	endif()
	add_test(NAME ${test_name} COMMAND ${CMAKE_COMMAND}
		-DRUNNER=$<TARGET_FILE:livelily_headless> -DLYV=${test_file} -DSNAPSHOT=${snapshot}
		-DOUT=${CMAKE_BINARY_DIR}/dumps/${test_name}.dump -DPYO=${LIVELILY_PYO} -P ${PROJECT_SOURCE_DIR}/tests/compare.cmake)
	set_tests_properties(${test_name} PROPERTIES SKIP_RETURN_CODE 77)
endforeach()
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/dumps")
