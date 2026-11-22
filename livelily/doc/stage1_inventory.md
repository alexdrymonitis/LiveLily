# Stage 1 inventory: separating the engine from openFrameworks

Stage 1 of the migration plan moves LiveLily's engine out of `ofApp` and away from openFrameworks, while the app keeps working.
This inventory sorts what is there today into engine, editor and front end, and turns stage 1 into an ordered list of small steps.
Each step is one commit, checked with the snapshot tests, which include the dry run of playback.

## Progress

- [x] Step 1: plain C++ utilities in the parser
- [x] Step 2: a color type of LiveLily's own (`color.h`, converted in `of_color.h`)
- [x] Step 3: interfaces for MIDI (`midi.h`), OSC (`osc.h`), serial (`serial.h`), audio (`audio.h`) and system commands (`system_command.h`), implemented with openFrameworks in the `of_*` files
- [ ] Step 4: `FrontendHost`
- [ ] Step 5: `Session`
- [ ] Step 6: split `Instrument`
- [ ] Step 7: split `SharedData`
- [ ] Step 8: headless tests

## Where the coupling is

- **`ofApp` is the engine and the front end at once.** The parser is already `ofApp` member functions (`parser.cpp`, 7,555 lines), next to the drawing, the keys, the panes and the shell (`ofApp.cpp`, 7,894 lines). `ofApp.h` declares about 400 members.
- **The parser barely uses openFrameworks.** Apart from the `ofApp::` qualifiers, `parser.cpp` uses `ofColor` 9 times, `ofxOscMessage` 8, `ofSoundDevice` 4, `ofToString` 3, `ofxMidiOut` 2, and single calls to `ofxOscSender`, `ofxOscReceiver`, `ofToDataPath`, `ofSystem`, `ofSetFrameRate`, `ofGetFrameRate`, `ofShowCursor`, `ofHideCursor` and `ofGetElapsedTimeMicros`.
- **`ofGetAppPtr()` is used 98 times**, from three files:

| File | Uses | What it reaches for |
| --- | --- | --- |
| `editor.cpp` | 72 | colors (`brightnessCoeff` 25, `foregroundColor`, `backgroundColor`), `commandsMap`, `keywords`, `commandsToNotTokenize`, `whichPane`, `parseStrings()`, completion (`getCommandCompletions()` etc., `pythonNames()`, `pythonSignature()`), alignment (`alignAfterEdit()`, `alignAfterPaste()`), `getActiveEditorNoteSpans()`, `initPyo()`, key forwarding (`keyPressed()`, `keyPressedOsc()` and releases), `sharedData.pyo` and the Python typing state, layout (`isScoreVisible()`, `scoreOrientation`, `sharedData.middleOfScreenX`) |
| `score.cpp` | 16 | `brightnessCoeff` 12, `lineWidth` 4 |
| `sequencer.cpp` | 10 | `parseCommand()` and `genCmdInput()` 4 each, `sendToParts()` 2, `callBoundFunction()`, `storeActiveEditorElement()`, `storeActiveEditorRest()`, `clearActiveEditorElement()` |

- **`Instrument` mixes music and drawing.** It holds the notes, durations, dynamics and MIDI data, and also the score objects `Staff` and `Notes` with their colors and layout, and the OSC sender of its score part.
- **`SharedData` (utils.h) mixes the sequencer's data with view state:** screen size, staff positions, piano roll geometry, beat visualization, and the coordinates of the area below the panes.

## Classification

### `ofApp` members

| Group | Members (examples) | Goes to |
| --- | --- | --- |
| Parser and commands | `parseStrings()`, `parseLines()`, `parseString()`, `evaluateTokens()`, `parseCommand()`, `prepareArgs()`, `commandTable()`, all `cmd...()` handlers, `parseMelodicLine()`, `parseBarLoop()`, `is...()` dispatchers, `nameKind()`, `commandSpec()`, `methodSpecs`, `genError()`/`genOutput()`/`genCmdInput()`, arithmetic, `applyToLine()`, tokenizing helpers | Engine (`Session`) |
| Session state | bars and loops (`storeNewBar()`, `storeNewLoop()`, parsing flags), `variables`, `listMap`/`listIndexes`, `functionIndexes` and function storing, `instGroups`, `mergeGroups`, `reservedNames`, `noteDurSettings`, `accOffset...`, `seqResolution`, `scoreDynamics` maps | Engine |
| Instruments | `initializeInstrument()`, `addedInstruments()`, `removeInstrument()`, `fillInMissingInsts()` | Engine (their score layout goes to the front end) |
| Sequencer | `sequencer`, `startSequencer`, `sharedData` (its sequencer part) | Engine |
| Input/output | `oscClients`, `oscServers`, `oscServerFuncs`, `oscReceiver`, `serial`, `midiPortOpen`, audio settings (`sampleRate`, `bufferSize`, channels, devices), `soundStream`, `pyoSet`, `initPyo()`, maestro (accelerometer) state | Engine, through platform interfaces |
| Score parts (OSC to other machines) | `sendToParts()` and the other `send...ToPart(s)()`, part bookkeeping | Engine (the messages), platform (the sockets) |
| Editor | `editors`, `whichPane`, `editorNoteSpans`, `activeEditorNotes`, alignment (`alignBars()` etc.), completion functions, `pythonTable`, tracebacks (`setEditorTraceback()`, pending tracebacks) | Editor model, in the core |
| Panes and shell | `numPanes`, `paneSplitOrientation`, `addPane()`, `removePane()`, `moveToPane()`, `paneBase`, shell (`shellStr`, `typeShellCommand()`, `executeShellCommand()`, `setShellOutput()`, scrolling), quit prompt, key state (`gPressed`, `dPressed`, `ctrlWPressed`) | Editor model (layout in lines), drawing in the front end |
| Drawing and window | `draw()`, `drawTraceback()`, `drawShell()`, `drawScore()`, `drawPianoRoll()`, `drawSequencer()`, `drawScope()`, score coordinates (`scoreXOffset`, `scoreOrientation`, `setScoreCoords()` etc.), fonts, colors, `brightnessCoeff`, `lineWidth`, window and mouse events | Front end |
| Colors of names | `commandsMap`, `colorNameMap`, `keywords`, `commandsToNotTokenize` | Core (a plain color type), used by both front ends |
| Test mode | `runTest()`, `dumpState()` | Core, so the tests run with no window (end of stage 1) |

### Commands that act on the front end

The engine runs every command, but some only change what is shown. These call the front end through an interface instead of reaching into it:
`\score` (show, hide, move, orientation, colors), `\editor`, `\cursor`, `\help`, `\livelily`/`\python`/`\lua` (the language of a pane), `\framerate`, `\active`/`\inactive`, `\sendkeys`/`\sendlines`, `\fromosc`, `\mouse`, `\mousex`, `\mousey` and `\pystr`.

### `SharedData`

| Part | Members (examples) | Goes to |
| --- | --- | --- |
| Music and sequencer | `instruments`, indexes and ordering of instruments, bars and loops, `loopData`, `barLines`, `tempo`, `tempoMs`, `numBeats`, `numerator`/`denominator`, `beatAtValues`, loop and bar positions, `functions`, mutexes, MIDI clock (`PPQN...`), `soundFonts`, `clickTrack`, `pyo` and Python typing | Engine |
| View | `screenWidth`/`screenHeight`, `middleOfScreen...`, staff positions (`staffXOffset`, `yStartPnts`, `barFirstStaffAnchor`...), note sizes, piano roll geometry, beat visualization (`beatViz...`), `tracebackYCoord...`, `showNotes`, animation flags | Front end (the sequencer's few writes become events) |

## Target structure for stage 1

- **`Session`** (engine): the parser, the session state, the instruments' musical data and the sequencer. Its methods are today's parser functions.
- **`FrontendHost`** (interface, implemented by `ofApp`): what the engine asks of the user interface. It covers:
    - error messages on lines;
    - highlighting the notes that play;
    - text typed from Python into a pane;
    - opening help;
    - the language of a pane;
    - the commands that act on the front end (above).
- **Platform interfaces** (implemented with openFrameworks for now, with JUCE in stage 4):
    - the clock (done, `clock.h`);
    - OSC;
    - MIDI;
    - serial;
    - audio;
    - running system commands.
- **`ofApp`**: the window, drawing, keys, panes and the shell, holding a `Session` and implementing `FrontendHost`.

## Steps, in order

1. **Plain C++ utilities in the parser.** Replace `ofToString`, `ofToDataPath` and `ofSystem` in `parser.cpp` with standard C++ and a path helper.
2. **A color type of LiveLily's own** for `commandsMap`, `colorNameMap` and the instruments' colors. It's converted to `ofColor` only where things are drawn.
3. **Platform interfaces for input/output.** OSC (clients, servers, score parts, the editor's OSC receiver), MIDI ports, serial, audio settings and system commands get small interfaces, implemented with openFrameworks for now. The sequencer's sends already go through one wrapper each (see the dry run), which become calls on these.
4. **`FrontendHost`.** The sequencer's calls into `ofApp` (the note highlights, bound functions) and the parser's calls into the editor and the score go through it.
5. **Move the session state into `Session`**, one group per commit:
    1. variables;
    2. lists;
    3. functions;
    4. bars and loops;
    5. instruments, groups and merges;
    6. input/output;
    7. Python;
    8. finally the parser functions themselves.

    `ofApp` keeps a `Session` and forwards to it until the front end uses `Session` directly.
6. **Split `Instrument`** into its musical data (engine) and its score view (`Staff`, `Notes`, colors, layout), which moves to the front end.
7. **Split `SharedData`** into the sequencer's data (engine) and the view state (front end).
8. **Headless tests.** Test mode builds a `Session` without a window, so the snapshot tests run with no openFrameworks at all. This marks the end of stage 1.

The editor's drawing (its cell grid) is stage 2, and is left as it is here.
