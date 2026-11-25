# one test of CTest (see CMakeLists.txt): runs a test file with the headless runner and compares its dump with the snapshot of the app's test mode
# in a CMake script so that it runs on every system; the headless runner has the app's panes, at the size of the app's test mode
# so the whole dump is compared (the screen too), except for keys that change what only the app has (see headless/main.cpp):
# the size of the font (Ctrl+= and Ctrl+-, which changes the size of the cells), up to the screen
# and the position of the score (Ctrl+P), which can change the active pane, up to the sections of the editor
#   cmake -DRUNNER=... -DLYV=... -DSNAPSHOT=... -DOUT=... -DPYO=ON|OFF -P compare.cmake
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

execute_process(COMMAND "${RUNNER}" --test "${LYV}" --out "${OUT}" RESULT_VARIABLE result OUTPUT_QUIET ERROR_QUIET)
if(NOT result EQUAL 0)
	message(FATAL_ERROR "the headless runner failed (${result}) on ${LYV}")
endif()

# the snapshot and the dump, with the same line endings, and up to where the keys of the file let them be compared
file(READ "${SNAPSHOT}" expected)
file(READ "${OUT}" actual)
string(REPLACE "\r\n" "\n" expected "${expected}")
string(REPLACE "\r\n" "\n" actual "${actual}")
string(REGEX MATCHALL "% test keys:[^\n]*" keyLines "${lyv}")
set(until "")
if(keyLines MATCHES "<C-[=-]>|<CtrlDown>[^<]*[=-]")
	set(until "=== screen\n")
endif()
if(keyLines MATCHES "<C-(A-)?(S-)?[pP]>")
	set(until "=== editor highlights\n")
endif()
foreach(var expected actual)
	if(until STREQUAL "")
		break()
	endif()
	string(FIND "${${var}}" "${until}" cut)
	if(cut GREATER -1)
		string(SUBSTRING "${${var}}" 0 ${cut} ${var})
	endif()
endforeach()

if(NOT expected STREQUAL actual)
	message(FATAL_ERROR "the dump differs from the snapshot: compare ${OUT} with ${SNAPSHOT}")
endif()
