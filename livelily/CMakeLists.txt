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

# FluidSynth, for SoundFonts (\instname.sndfont): with pkg-config (Linux, macOS), as a CMake package (e.g. vcpkg on Windows)
# or as a library file of its own
# pkg-config can find FluidSynth and still give neither its flags nor its libraries, when a package it requires isn't in pkg-config's
# path (e.g. Homebrew's readline on macOS), so its answer counts only if it gives the libraries
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
	pkg_check_modules(FLUIDSYNTH IMPORTED_TARGET fluidsynth)
endif()
if(TARGET PkgConfig::FLUIDSYNTH AND FLUIDSYNTH_LINK_LIBRARIES)
	set(fluidsynth_target PkgConfig::FLUIDSYNTH)
	set(fluidsynth_found "pkg-config (${FLUIDSYNTH_LINK_LIBRARIES})")
else()
	find_package(FluidSynth CONFIG QUIET)
	if(TARGET FluidSynth::libfluidsynth)
		set(fluidsynth_target FluidSynth::libfluidsynth)
		set(fluidsynth_found "its CMake package (${FluidSynth_DIR})")
	else()
		find_library(LIVELILY_FLUIDSYNTH_LIBRARY NAMES fluidsynth libfluidsynth HINTS ${FLUIDSYNTH_LIBDIR})
		if(NOT LIVELILY_FLUIDSYNTH_LIBRARY)
			message(FATAL_ERROR "FluidSynth wasn't found: install it (e.g. libfluidsynth-dev, Homebrew's fluid-synth, or vcpkg's fluidsynth)")
		endif()
		set(fluidsynth_target "${LIVELILY_FLUIDSYNTH_LIBRARY}")
		set(fluidsynth_found "its library (${LIVELILY_FLUIDSYNTH_LIBRARY})")
	endif()
endif()
# the directory of fluidsynth.h, which none of these ways always gives (e.g. the pkg-config of Homebrew above, or FluidSynth's CMake
# package), so it is looked for too, from pkg-config's includedir and next to the package (lib/cmake/fluidsynth -> include)
find_path(LIVELILY_FLUIDSYNTH_INCLUDE_DIR fluidsynth.h HINTS ${FLUIDSYNTH_INCLUDEDIR} ${FLUIDSYNTH_INCLUDE_DIRS} "${FluidSynth_DIR}/../../../include")
if(NOT LIVELILY_FLUIDSYNTH_INCLUDE_DIR)
	message(FATAL_ERROR "FluidSynth was found with ${fluidsynth_found}, but not fluidsynth.h: give its directory with -DLIVELILY_FLUIDSYNTH_INCLUDE_DIR=...")
endif()
message(STATUS "FluidSynth: found with ${fluidsynth_found}, and fluidsynth.h in ${LIVELILY_FLUIDSYNTH_INCLUDE_DIR}")
add_library(livelily_fluidsynth INTERFACE)
target_link_libraries(livelily_fluidsynth INTERFACE ${fluidsynth_target})
target_include_directories(livelily_fluidsynth INTERFACE "${LIVELILY_FLUIDSYNTH_INCLUDE_DIR}")
set(LIVELILY_FLUIDSYNTH livelily_fluidsynth)

# Python, embedded to run pyo
if(LIVELILY_PYO)
	find_package(Python3 COMPONENTS Interpreter Development.Embed REQUIRED)
endif()

# JUCE, for the platform (MIDI and audio devices, see platform/), from a directory given (e.g. a JUCE checkout), or fetched at a fixed version
# (AGPLv3, see the migration plan); its helper juceaide is built when CMake configures
set(LIVELILY_JUCE_DIR "" CACHE PATH "A directory with JUCE to build with, instead of fetching JUCE")
if(LIVELILY_JUCE_DIR)
	add_subdirectory("${LIVELILY_JUCE_DIR}" "${CMAKE_BINARY_DIR}/juce" EXCLUDE_FROM_ALL)
else()
	include(FetchContent)
	FetchContent_Declare(juce GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.12 GIT_SHALLOW TRUE)
	FetchContent_MakeAvailable(juce)
endif()

# the platform of LiveLily with JUCE: MIDI output (in-house on Linux) and audio devices, compiled into every program that links it, with the JUCE modules it needs
add_library(livelily_platform_juce INTERFACE)
target_sources(livelily_platform_juce INTERFACE "${PROJECT_SOURCE_DIR}/platform/juce_audio.cpp" "${PROJECT_SOURCE_DIR}/platform/juce_platform.cpp")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	# MIDI with the ALSA sequencer, as JUCE's MIDI sends nothing with kernels whose sequencer has no MIDI 2.0 (see alsa_midi.cpp)
	find_package(ALSA REQUIRED)
	target_sources(livelily_platform_juce INTERFACE "${PROJECT_SOURCE_DIR}/platform/alsa_midi.cpp")
	target_link_libraries(livelily_platform_juce INTERFACE ALSA::ALSA)
else()
	target_sources(livelily_platform_juce INTERFACE "${PROJECT_SOURCE_DIR}/platform/juce_midi.cpp")
endif()
target_include_directories(livelily_platform_juce INTERFACE "${PROJECT_SOURCE_DIR}/platform")
target_link_libraries(livelily_platform_juce INTERFACE juce::juce_audio_devices juce::juce_recommended_config_flags)
target_compile_definitions(livelily_platform_juce INTERFACE
	JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 JUCE_STANDALONE_APPLICATION=1 JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	# JACK, besides ALSA: JUCE loads the JACK library when the program runs, and needs only its headers to build
	target_compile_definitions(livelily_platform_juce INTERFACE JUCE_JACK=1)
endif()

# the core: the engine (session, parser, sequencer, instruments), the editor, its grids, and the keys
set(LIVELILY_CORE_SOURCES
	src/canvas.cpp src/cellgrid.cpp src/clock.cpp src/codetext.cpp src/editor.cpp src/function.cpp src/instrument.cpp src/keys.cpp src/midifile.cpp src/musicxml.cpp src/osc.cpp src/osc_packet.cpp src/osc_udp.cpp
	src/parser.cpp src/paths.cpp src/score.cpp src/score_view.cpp src/sequencer.cpp src/session.cpp src/session_dump.cpp src/session_setup.cpp src/soundfont.cpp
	src/serial_port.cpp src/system_command.cpp src/test_midi.cpp src/test_run.cpp src/udp.cpp src/workspace.cpp src/workspace_alignment.cpp src/workspace_editors.cpp src/workspace_keys.cpp src/workspace_shell.cpp)
if(LIVELILY_PYO)
	list(APPEND LIVELILY_CORE_SOURCES src/PyoClass.cpp src/pyo_classes.cpp)
endif()
add_library(livelily_core STATIC ${LIVELILY_CORE_SOURCES})
target_include_directories(livelily_core PUBLIC src)
find_package(Threads REQUIRED)
# zlib, for compressed MusicXML files (.mxl, see musicxml.cpp), which the editor loads
find_package(ZLIB REQUIRED)
target_link_libraries(livelily_core PUBLIC ${LIVELILY_FLUIDSYNTH} Threads::Threads ZLIB::ZLIB)
if(LIVELILY_PYO)
	target_compile_definitions(livelily_core PUBLIC USEPYO)
	target_link_libraries(livelily_core PUBLIC Python3::Python)
endif()
if(WIN32)
	# the Documents folder (SHGetKnownFolderPath(), see paths.cpp), and Winsock for OSC (see udp.cpp)
	target_link_libraries(livelily_core PUBLIC shell32 ole32 ws2_32)
endif()
if(MSVC)
	target_compile_options(livelily_core PRIVATE /W3 /utf-8)
	target_compile_definitions(livelily_core PUBLIC _USE_MATH_DEFINES NOMINMAX _CRT_SECURE_NO_WARNINGS)
else()
	target_compile_options(livelily_core PRIVATE -Wall -Werror=return-type)
endif()

# the headless runner
add_executable(livelily_headless headless/main.cpp headless/platform.cpp)
target_link_libraries(livelily_headless PRIVATE livelily_core livelily_platform_juce)
# the headless runner runs JUCE's message loop for a moment to find the MIDI ports (see platform.h)
target_compile_definitions(livelily_headless PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1)
set_target_properties(livelily_headless PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")
foreach(config ${CMAKE_CONFIGURATION_TYPES})
	string(TOUPPER ${config} CONFIG)
	set_target_properties(livelily_headless PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${CONFIG} "${PROJECT_SOURCE_DIR}/bin")
endforeach()

# the tests: every test file (tests/private/lyv/*.lyv) with a snapshot in tests/private/snapshots runs in the headless runner
# (the test files are kept out of the published repository, so there the tests are only built and none runs)
# and its dump is compared with the snapshot up to the sections of the editor (see tests/compare.cmake)
# the snapshots are made by the app's test mode (tests/snapshots.sh save), which also checks the editor's sections and the screen
enable_testing()
# OSC: packets, and messages through a socket (with no test files, so it runs everywhere)
add_executable(livelily_osc_test tests/osc_test.cpp)
target_link_libraries(livelily_osc_test PRIVATE livelily_core)
add_test(NAME osc COMMAND livelily_osc_test)
file(GLOB LIVELILY_TEST_FILES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/tests/private/lyv/*.lyv")
foreach(test_file ${LIVELILY_TEST_FILES})
	get_filename_component(test_name "${test_file}" NAME_WE)
	set(snapshot "${PROJECT_SOURCE_DIR}/tests/private/snapshots/${test_name}.dump")
	if(NOT EXISTS "${snapshot}")
		continue()
	endif()
	add_test(NAME ${test_name} COMMAND ${CMAKE_COMMAND}
		-DRUNNER=$<TARGET_FILE:livelily_headless> -DLYV=${test_file} -DSNAPSHOT=${snapshot}
		-DOUT=${CMAKE_BINARY_DIR}/dumps/${test_name}.dump -DPYO=${LIVELILY_PYO} -P ${PROJECT_SOURCE_DIR}/tests/compare.cmake)
	set_tests_properties(${test_name} PROPERTIES SKIP_RETURN_CODE 77)
endforeach()
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/dumps")
