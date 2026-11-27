#!/bin/bash
# screenshots of the test files, for checking that a change to the drawing (the score, the views, the panes) doesn't change what is drawn
# which the snapshots don't see: every .lyv file of tests/private/lyv runs in the test mode of LiveLily (see src/test_run.h)
# and the window is saved when the test has run, as a PNG file
#   tests/screenshots.sh save name       saves the screenshots to tests/current/screens/name (e.g. before a change)
#   tests/screenshots.sh compare a b     compares tests/current/screens/a with tests/current/screens/b, pixel by pixel (with ImageMagick)
#   tests/screenshots.sh side a b        puts the screenshots of a and b side by side, in tests/current/screens/a_b (e.g. to look at the changes
#                                        that compare finds)
# build the program first (cmake --build build)

testsDir="$(cd "$(dirname "$0")" && pwd)"
appDir="$(dirname "$testsDir")"
binDir="$appDir/bin"
screensDir="$testsDir/current/screens"
program=livelily

case "$1" in
	save)
		[ -n "$2" ] || { echo "usage: $0 save name"; exit 2; }
		if [ ! -x "$binDir/$program" ]; then
			echo "$binDir/$program not found, build the program first"
			exit 2
		fi
		outDir="$screensDir/$2"
		rm -rf "$outDir"
		mkdir -p "$outDir"
		for file in "$testsDir"/private/lyv/*.lyv; do
			name="$(basename "$file" .lyv)"
			(cd "$binDir" && timeout 120 "./$program" --test "$file" --out "$outDir/$name.dump" --screenshot "$outDir/$name.png" > "$outDir/$name.log" 2>&1)
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
	side)
		[ -n "$2" ] && [ -n "$3" ] || { echo "usage: $0 side a b"; exit 2; }
		a="$screensDir/$2"
		b="$screensDir/$3"
		outDir="$screensDir/$2_$3"
		rm -rf "$outDir"
		mkdir -p "$outDir"
		for png in "$a"/*.png; do
			name="$(basename "$png" .png)"
			[ -f "$b/$name.png" ] || { echo "MISSING  $name"; continue; }
			# a with b to its right, with a gray line between them
			convert "$png" -background gray50 -gravity east -splice 4x0 "$b/$name.png" +append "$outDir/$name.png"
		done
		echo "saved $(ls "$outDir"/*.png 2>/dev/null | wc -l) pictures to $outDir ($2 on the left, $3 on the right)"
		;;
	*)
		echo "usage: $0 save name | compare a b | side a b"
		exit 2
		;;
esac
