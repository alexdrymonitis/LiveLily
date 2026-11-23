#!/bin/bash
# builds the headless test runner (see main.cpp) into bin/livelily_headless, from the engine's sources and no openFrameworks
# with Python (pyo) and FluidSynth, like `make wpyo` builds the app, and RtMidi from the ofxMidi addon, with ALSA on Linux
set -e
cd "$(dirname "$0")/.."
RTMIDI=../../../addons/ofxMidi/libs/rtmidi
ENGINE="clock codetext function instrument osc parser PyoClass pyo_classes sequencer session session_dump session_setup soundfont system_command test_midi"
mkdir -p obj/headless
objs=()
for name in $ENGINE; do objs+=("src/$name.cpp"); done
objs+=(headless/main.cpp headless/platform.cpp "$RTMIDI/RtMidi.cpp")
pids=()
outs=()
for src in "${objs[@]}"; do
	obj="obj/headless/$(basename "${src%.cpp}").o"
	outs+=("$obj")
	# compile only what changed (the source, or any header of src)
	if [ ! -f "$obj" ] || [ "$src" -nt "$obj" ] || [ -n "$(find src headless -name '*.h' -newer "$obj" -print -quit)" ]; then
		g++ -c -O2 -std=c++20 -Wall -Werror=return-type -DUSEPYO -D__LINUX_ALSA__ $(python3-config --cflags) -Isrc -Iheadless/compat -I"$RTMIDI" "$src" -o "$obj" &
		pids+=($!)
		# at most two at a time, like make -j2
		if [ ${#pids[@]} -ge 2 ]; then wait "${pids[0]}"; pids=("${pids[@]:1}"); fi
	fi
done
for pid in "${pids[@]}"; do wait "$pid"; done
g++ -o bin/livelily_headless "${outs[@]}" $(python3-config --ldflags --embed) -lfluidsynth -lasound -pthread
echo "built bin/livelily_headless"
