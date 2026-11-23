# one test of CTest (see CMakeLists.txt): runs a test file with the headless runner and compares its dump with the snapshot of the app's test mode
# up to the sections of the editor, which need the app (as tests/headless.sh does), in a CMake script so that it runs on every system
#   cmake -DRUNNER=... -DLYV=... -DSNAPSHOT=... -DDATA=... -DOUT=... -DPYO=ON|OFF -P compare.cmake
# a test file that needs what this machine doesn't have is skipped (exit code 77): pyo in a build without it
# or a SoundFont at an absolute path (or one in the home directory) that isn't there

file(READ "${LYV}" lyv)

function(skip reason)
	message("skipped: ${reason}")
	if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.29)
		cmake_language(EXIT 77)
	endif()
	return()
endfunction()

if(NOT PYO AND lyv MATCHES "(sendto python|\\\\python|\\\\initpyo)")
	skip("the test needs pyo, and this build is without it")
	return()
endif()
string(REGEX MATCHALL "\\.sndfont \"[^\"]+\"" soundfonts "${lyv}")
foreach(soundfont ${soundfonts})
	string(REGEX REPLACE "^\\.sndfont \"(.*)\"$" "\\1" path "${soundfont}")
	if(path MATCHES "^~")
		if(DEFINED ENV{HOME})
			string(REGEX REPLACE "^~" "$ENV{HOME}" path "${path}")
		elseif(DEFINED ENV{USERPROFILE})
			string(REGEX REPLACE "^~" "$ENV{USERPROFILE}" path "${path}")
		endif()
	endif()
	if(IS_ABSOLUTE "${path}" AND NOT EXISTS "${path}")
		skip("the test needs the SoundFont ${path}, which this machine doesn't have")
		return()
	endif()
endforeach()

execute_process(COMMAND "${RUNNER}" --test "${LYV}" --out "${OUT}" --data "${DATA}" RESULT_VARIABLE result OUTPUT_QUIET ERROR_QUIET)
if(NOT result EQUAL 0)
	message(FATAL_ERROR "the headless runner failed (${result}) on ${LYV}")
endif()

# the snapshot up to the sections of the editor, and the dump, with the same line endings
file(READ "${SNAPSHOT}" expected)
file(READ "${OUT}" actual)
string(REPLACE "\r\n" "\n" expected "${expected}")
string(REPLACE "\r\n" "\n" actual "${actual}")
string(FIND "${expected}" "=== editor highlights\n" cut)
if(cut GREATER -1)
	string(SUBSTRING "${expected}" 0 ${cut} expected)
endif()
# \editor.alignment rewrites the lines in the editor of the app, and the bars keep their text as it was rewritten
# so for the files that turn it on, the text of the bars is left out, and everything else is compared
if(lyv MATCHES "\\\\editor\\.alignment (on|short|long)")
	string(REGEX REPLACE "\n  text [^\n]*" "" expected "${expected}")
	string(REGEX REPLACE "\n  text [^\n]*" "" actual "${actual}")
endif()

if(NOT expected STREQUAL actual)
	message(FATAL_ERROR "the dump differs from the snapshot: compare ${OUT} with ${SNAPSHOT}")
endif()
