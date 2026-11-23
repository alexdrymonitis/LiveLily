# Stage 2 inventory: the editor renders into a cell grid

Stage 2 of the migration plan makes the editor draw into a grid of cells instead of drawing with openFrameworks, inside the openFrameworks app, with no visible change.
The desktop app then paints that grid, and stage 5 paints the same grid in a terminal.
Each step is one commit, checked with the snapshot tests, the headless tests, and before/after screenshots, which must be identical.

## Progress

- [x] Step 1: the cell grid (`cellgrid.h`) and its openFrameworks painter (`of_grid_painter.h/.cpp`)
- [x] Step 2: the text of a pane in the grid, with the line numbers, the cursor and the note highlights: `Editor::drawText()` fills `textGrid` and `gutterGrid` and paints them, and the screenshots of every test file are the same
- [x] Step 3: the completion pop-up in the grid (`completionGrid`, placed by pixels, with the text inset by half a character)
- [x] Step 4: the status line of a pane in the grid (`statusGrid`: the text in cells, its three parts painted where they were)
- [x] Step 5: the area below the panes in the grid (`ofApp::belowGrid`: the messages of the lines, or the shell with its cursor)
- [x] Step 6: the screen in the test dump (`=== screen`, written by `dumpGrid()` in `cellgrid.cpp` a few frames after the file has run); the messages below the panes are now in the same order on every run, so the screen and the screenshots are the same every time
- [x] Step 7: `EditorHost` (`editor_host.h`, implemented by `ofApp`): the editor no longer uses `ofGetAppPtr()`; its time is `Clock`, and the clipboard, file dialogs and window size come from the host. What is left of openFrameworks in the editor is its font and the painting (step 9)
- [ ] Step 8: key events
- [ ] Step 9: the editor without openFrameworks

## What the editor draws today

`Editor::drawText()` (about 560 lines of `editor.cpp`) draws a pane, in this order:

1. the background of the lines with an error or a warning (red, orange), over the width of the text area;
2. the cyan flash of lines that run, fading with its alpha;
3. the magenta cell of the bracket matching the one under the cursor;
4. the text, token by token, in the colors of the language (commands, methods, names, numbers, strings, comments), with the cursor drawn in the color of the token it is on;
5. the selection: a goldenrod background with the selected text in black;
6. the note highlights: outlines around the tokens of the notes that play (`\editor.animation`);
7. the cursor, if the token loop didn't draw it: a block (normal mode), a bar (insert mode), an underline (after `d`), or an outline (inactive pane);
8. the hint of the arguments of a Python callable, in a color between the foreground and the background;
9. the character under a block cursor, in black;
10. the line numbers, right-aligned.

`Editor::drawPaneSeparator()` draws the status line (pane number, file name or file error, the mode), and `Editor::drawCompletions()` the completion pop-up.
`ofApp` draws the area below the panes: the messages of the lines (`drawTraceback()`) or the shell (`drawShell()`).

## Why a grid can paint it exactly

- **Everything is on columns.** The font is monospaced, the editor keeps its width as a whole number of pixels (`Editor::oneCharacterWidth`), and every token, rectangle and cursor is placed at `column * oneCharacterWidth`.
- **The glyphs are on columns too.** DroidSansMono's advances, loaded as openFrameworks loads it (autohinted), are whole pixels at every size, equal to `oneCharacterWidth` (checked from 10 to 40 points). So a glyph lands on the same pixel whether it is drawn by token, by run or by cell.
- **Rows are cells.** Every row is `cursorHeight` high, and the text sits on a baseline at a fixed offset in it.

What doesn't fall on cells stays in pixels in the desktop painter only:
- the half-character margins (the text starts one and a half characters after the line numbers, the status line's mode ends 10 pixels before the edge);
- backgrounds of whole rows that end at the pane's edge, not at a column.

The grid keeps these as regions of a pane (gutter, text, status line) with their own origins, and a row background that reaches the end of its region.
The terminal front end lays out the same regions in whole cells.

## The grid

- **Cell:** a character (UTF-8), a foreground color, a background color or none, an overlay color with alpha or none (the flash of running lines), and an outline flag (note highlights).
- **Grid:** columns, rows and cells, plus a cursor (row, column, shape, color) and, for the desktop, the pixel geometry of its regions.
- **Painting order** keeps today's: backgrounds, overlays, text, selection text, outlines, cursor, the character under the cursor. The painter draws runs of cells with the same colors with one call, as the editor draws tokens now.
- **Colors** stay LiveLily's `Color` in the grid, and the brightness coefficient is applied by the painter, as every `ofSetColor()` of the editor does now.

## Where the editor reaches the app

`editor.cpp` reaches the app with `ofGetAppPtr()` 86 times:

| What | Uses |
| --- | --- |
| colors: `brightnessCoeff`, `foregroundColor`, `backgroundColor` | 33 |
| name tables of the session: `commandsMap`, `keywords`, `commandsToNotTokenize` | 12 |
| Python: `sharedData.pyo`, `initPyo()`, `pythonNames()`, `pythonSignature()`, the typing state | 12 |
| the active pane, and layout: `whichPane`, `isScoreVisible()`, `scoreOrientation`, `middleOfScreenX` | 10 |
| completion: `getCommandCompletions()`, `getMethodCompletions()`, `getArgumentCompletions()` | 3 |
| alignment: `alignAfterEdit()`, `alignAfterPaste()`, `startsMelodicLine()` | 3 |
| running code: `parseStrings()` | 1 |
| note highlights: `getActiveEditorNoteSpans()` | 1 |
| keys forwarded to other panes and machines: `keyPressed()`, `keyReleased()`, `keyPressedOsc()`, `keyReleasedOsc()` | 4 |

It also uses openFrameworks for time (`ofGetElapsedTimeMillis()`, 14), the file dialogs (`ofSystemLoadDialog()`, `ofSystemSaveDialog()`), the font and its drawing, and `ofGetWindowPtr()` (3).

## Steps, in order

1. **The cell grid and its openFrameworks painter.** `cellgrid.h` (no openFrameworks) and `of_grid_painter.h/.cpp`, which paints a grid with the editor's font. Nothing uses them yet.
2. **The text of a pane in the grid.** `drawText()` fills the grid with the text, the row backgrounds, the flash, the bracket cell, the selection, the argument hint and the line numbers, and the app paints it. The screenshots of every test file stay identical.
3. **The completion pop-up in the grid.** (The cursor and the note highlights went with step 2, as the cursor is drawn in the middle of the text.)
4. **The status line of a pane in the grid.**
5. **The area below the panes in the grid:** the messages of the lines and the shell, with its input line and cursor.
6. **The screen in the test dump.** Test mode writes the grids of the panes and of the area below them as text (a `=== screen` section, with the colors as letters on a second layer), so that the snapshots test what the editor shows.
7. **`EditorHost`.** The editor reaches the app only through an interface for the uses above; time goes through `Clock`, the dialogs through the host, and the name tables come from the `Session`.
8. **Key events.** The editor takes key events of its own (key, modifiers, press or release), which the app makes from openFrameworks' keys, so that the terminal front end can make them too.
9. **The editor without openFrameworks.** The editor and its grid compile with no openFrameworks, and the headless runner uses the real editor, so `tests/headless.sh` compares the whole dump, editor sections and screen included. This ends stage 2.

The panes' layout (their sizes in pixels, the score beside them) stays in the app in this stage; it is laid out in cells in stage 5, where the terminal needs it.
