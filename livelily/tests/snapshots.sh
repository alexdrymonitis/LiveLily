#!/bin/bash
# runs every .lyv file of tests/private/lyv in the test mode of LiveLily (see src/testmode.cpp) and saves or compares what the parser makes of it
#   tests/snapshots.sh save     saves the dumps to tests/private/snapshots, as the reference (run it before a change to the parser)
#   tests/snapshots.sh check    dumps to tests/current and compares with tests/private/snapshots (run it after a change)
# a file whose run crashes or doesn't finish is reported, and so is every file whose dump differs from its snapshot
# build the program first (make wpyo -j2)
# LIVELILY_APP=livelily_desktop tests/snapshots.sh check   runs the JUCE app (built with CMake) instead of the openFrameworks app

mode="$1"
if [ "$mode" != "save" ] && [ "$mode" != "check" ]; then
	echo "usage: $0 save|check"
	exit 2
fi

testsDir="$(cd "$(dirname "$0")" && pwd)"
appDir="$(dirname "$testsDir")"
binDir="$appDir/bin"
program="${LIVELILY_APP:-livelily}"
if [ ! -x "$binDir/$program" ]; then
	echo "$binDir/$program not found, build the program first"
	exit 2
fi
# a binary older than the sources (e.g. after a build that failed) would check the old code, so it is refused
newerSource="$(find "$appDir/src" "$appDir/desktop" -newer "$binDir/$program" \( -name '*.cpp' -o -name '*.h' \) 2>/dev/null | head -1)"
if [ -n "$newerSource" ]; then
	echo "$binDir/$program is older than $newerSource, build the program first"
	exit 2
fi
if [ "$mode" = "save" ]; then
	outDir="$testsDir/private/snapshots"
else
	outDir="$testsDir/current"
fi
mkdir -p "$outDir"
rm -f "$outDir"/*.dump

failed=0
# the files of tests/private/lyv (a private repository, kept out of the published one), which check the parser, the sequencer and the editor
for file in "$testsDir"/private/lyv/*.lyv; do
	name="$(basename "$file" .lyv)"
	# the program finds LiveLily's resources from its own path (see src/paths.h), and is run from bin as before
	(cd "$binDir" && timeout 120 "./$program" --test "$file" --out "$outDir/$name.dump" > "$outDir/$name.log" 2>&1)
	status=$?
	if [ $status -ne 0 ]; then
		if [ $status -eq 124 ]; then echo "TIMEOUT  $name"; else echo "CRASH    $name (exit $status, see $outDir/$name.log)"; fi
		failed=1
		continue
	fi
	rm -f "$outDir/$name.log"
	if [ "$mode" = "check" ]; then
		if [ ! -f "$testsDir/private/snapshots/$name.dump" ]; then
			echo "NEW      $name (no snapshot)"
		elif ! diff -q "$testsDir/private/snapshots/$name.dump" "$outDir/$name.dump" > /dev/null; then
			echo "DIFFERS  $name (diff tests/private/snapshots/$name.dump tests/current/$name.dump)"
			failed=1
		else
			echo "ok       $name"
		fi
	else
		echo "saved    $name"
	fi
done
exit $failed
