#!/bin/bash
# runs every .lyv file of bin/data and tests/lyv with the headless runner (see headless/main.cpp), which has no window and no openFrameworks
# and compares its dumps with the snapshots of the app's test mode (see snapshots.sh), up to the sections of the editor, which need the app
#   tests/headless.sh            builds the runner (see headless/build.sh) and checks every file that has a snapshot
cd "$(dirname "$0")/.."
headless/build.sh > /dev/null || exit 1
mkdir -p tests/current/headless
failed=0
for f in bin/data/*.lyv tests/lyv/*.lyv; do
	name=$(basename "$f" .lyv)
	snapshot="tests/snapshots/$name.dump"
	[ -f "$snapshot" ] || continue
	dump="tests/current/headless/$name.dump"
	timeout 120 bin/livelily_headless --test "$PWD/$f" --out "$dump" > /dev/null 2>&1
	# \editor.alignment rewrites the lines in the editor of the app, and the bars keep their text as it was rewritten
	# so for the files that turn it on, the text of the bars is left out, and everything else is compared
	textFilter='^$'
	grep -qE '\\editor\.alignment (on|short|long)' "$f" && textFilter='^  text '
	if diff -q <(sed '/^=== editor highlights$/,$d' "$snapshot" | grep -vE "$textFilter") <(grep -vE "$textFilter" "$dump") > /dev/null; then
		echo "ok       $name"
	else
		echo "DIFF     $name"
		failed=1
	fi
done
exit $failed
