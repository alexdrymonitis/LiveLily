#!/bin/bash
# screenshots of the test files, for checking that a change to the drawing (the score, the views, the panes) doesn't change what is drawn
# which the snapshots don't see: every .lyv file of tests/private/lyv runs in the test mode of LiveLily (see src/test_run.h)
# and the window is saved when the test has run, as a PNG file
#   tests/screenshots.sh save name       saves the screenshots to tests/current/screens/name (e.g. before a change)
#   tests/screenshots.sh compare a b     compares tests/current/screens/a with tests/current/screens/b, pixel by pixel (with ImageMagick)
# build the program first (make wpyo -j2)

testsDir="$(cd "$(dirname "$0")" && pwd)"
appDir="$(dirname "$testsDir")"
binDir="$appDir/bin"
screensDir="$testsDir/current/screens"

case "$1" in
	save)
		[ -n "$2" ] || { echo "usage: $0 save name"; exit 2; }
		if [ ! -x "$binDir/livelily" ]; then
			echo "$binDir/livelily not found, build the program first"
			exit 2
		fi
		outDir="$screensDir/$2"
		rm -rf "$outDir"
		mkdir -p "$outDir"
		for file in "$testsDir"/private/lyv/*.lyv; do
			name="$(basename "$file" .lyv)"
			(cd "$binDir" && timeout 120 ./livelily --test "$file" --out "$outDir/$name.dump" --screenshot "$outDir/$name.png" > "$outDir/$name.log" 2>&1)
			[ -f "$outDir/$name.png" ] || echo "no screenshot: $name"
		done
		echo "saved $(ls "$outDir"/*.png 2>/dev/null | wc -l) screenshots to $outDir"
		;;
	compare)
		[ -n "$2" ] && [ -n "$3" ] || { echo "usage: $0 compare a b"; exit 2; }
		a="$screensDir/$2"
		b="$screensDir/$3"
		differ=0
		for png in "$a"/*.png; do
			name="$(basename "$png" .png)"
			if [ ! -f "$b/$name.png" ]; then
				echo "MISSING  $name"
				differ=1
				continue
			fi
			# the number of pixels that differ (ImageMagick writes it to the error stream)
			pixels="$(compare -metric AE "$png" "$b/$name.png" null: 2>&1)"
			if [ "$pixels" != "0" ]; then
				echo "DIFFERS  $name ($pixels pixels, see: compare $png $b/$name.png diff.png)"
				differ=1
			fi
		done
		[ $differ -eq 0 ] && echo "all the screenshots are the same"
		exit $differ
		;;
	*)
		echo "usage: $0 save name | compare a b"
		exit 2
		;;
esac
