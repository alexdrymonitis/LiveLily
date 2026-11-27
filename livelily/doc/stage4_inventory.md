# Stage 4 inventory: JUCE for the platform and the desktop app

Stage 4 of the migration plan replaces openFrameworks with JUCE: the platform interfaces get implementations of their own or JUCE's,
the logic of the front end leaves `ofApp` for the core, the views draw through an interface, and a JUCE desktop app takes the place of the openFrameworks app.
Each step is one commit, checked with the snapshot tests of the app and the headless tests, and the openFrameworks app keeps working until the last step.

## Progress

- [x] Step 1: OSC in-house (`osc_packet.cpp`, `udp.cpp`, `osc_udp.cpp`), in the app and in the headless runner, with the CTest test `osc`; checked live with `\oscclient`, `\oscserver` and `\sendkeys`/`\sendlines` (the booleans sent to LiveLily-part are still to be checked, when LiveLily-part is migrated)
- [x] Step 2: JUCE in CMake (`LIVELILY_JUCE_DIR`, or fetched at 8.0.12), audio with JUCE and MIDI with the ALSA sequencer on Linux (JUCE's on macOS and Windows) in `platform/`, used by `livelily_headless --play`; RtMidi is gone; checked live with MIDI to Pd and the click track, with and without JACK
- [x] Step 3: serial ports in-house (`serial_port.cpp`: termios on Linux and macOS, Win32 on Windows), in the app and in the headless runner; **still to be checked live with a board** (`\listserialports`, `\openserialport`, `\serialwrite`, `\serialprint`, `\closeserialport`)
- [x] Step 4: the panes, their layout and the shell area leave `ofApp` for the core (`Workspace`, in `workspace.h`, `workspace.cpp` and `workspace_shell.cpp`, with `WorkspaceHost` for what it asks of the front end; `ofApp` keeps aliases of its state until the later steps); two older bugs with the score below or above the panes fixed on the way (`:sp` with one pane, and the focus when the score moves to the bottom row); checked live
- [x] Step 5: the key dispatch and the quit prompt leave `ofApp` (`workspace_keys.cpp`; `WorkspaceHost` changes the font, the score, the brightness and full screen for the keys); closing a pane with changes that are not saved asks first, as quitting does; Ctrl+n closes all panes but one (it closed only half of them); checked live
- [x] Step 6: alignment, the note highlights and test mode leave `ofApp`; the headless tests check the editor's sections, and the screen
  - [x] 6a: the workspace answers what the editors and the engine ask (`workspace_editors.cpp`, `workspace_alignment.cpp`): running lines, the messages of lines, the note highlights, autocompletion, alignment; Ctrl+n asks to save, stops the sequencer before clearing the session (it crashed while playing), and leaves an untitled file (`Editor::newFile()`); checked live
  - [x] 6b: test mode in the core (`test_run.cpp`), and the headless runner with the panes, as large as the app's in test mode (1350 by 900 pixels, cells of 11 by 22), typing the keys of test files; CTest compares the whole dump with the app's snapshot (60 of 63 match it whole), up to the screen for keys that change the font size, and up to the editor's sections for keys that move the score (Ctrl+P), which the app's view code does until step 7
- [x] Step 7: the views (score, piano roll, sequencer, scope) draw through an interface, painted by openFrameworks with no visible change
  - [x] 7a: the canvas (`canvas.h`, `of_canvas.cpp`), and the score drawing through it (`score.cpp`, `score_view.cpp`, which was `of_instrument_display.cpp`), in the core; the screenshots are the same (`tests/screenshots.sh`); the repeat signs are drawn in horizontal view too, by the place of every bar in its loop; checked live
  - [x] 7b: the views of `ofApp` (the layout and drawing of the score, the piano roll, the sequencer, the oscilloscope, the beat) in the core (`Views`, in `views.cpp`), drawing through the canvas; the screenshots are the same; the headless runner lays out the views as the app does (with a canvas that draws nothing), so CTest compares whole dumps except for keys that change the size of the font (62 of 63 match whole); the line that separates the score from the panes, with inverted colors, stops at the area below the panes; checked live
- [x] Step 8: the JUCE desktop app (window, grid painter, keys, clipboard, file dialogs, mouse, OpenGL, Bravura)
  - [x] 8a: LiveLily in the core (`App`, in `app.cpp`): the session, the panes and the views, what they ask of each other, every frame (OSC, typing from Python, the oscilloscope), and drawing them; a front end gives it a window (`AppWindow`, in `app_window.h`: the editor's font, painting grids, clipboard, file dialogs, mouse, frame rate, full screen, quitting); `ofApp` is only the window of openFrameworks, and the headless runner uses `App` with a window that paints nothing; members of `App` and `Views` have values before `setup()` (the panes were laid out with whatever `showScore` held)
  - [x] 8b: Bravura in place of Sonata (`resources/fonts/Bravura.otf`, SIL OFL; the glyphs by their SMuFL code points in `smufl.h`), in the openFrameworks app: clefs on their lines, flags at the end of the stem (one glyph per duration), rests on the lines, accidentals by their width, the quarter tones of Stein and Zimmermann, and stems up at the edge of the notehead; the canvas loads music fonts with `loadMusicFont()` (openFrameworks loads only the ranges it is given); checked live
  - [x] 8c: the JUCE desktop app (`desktop/`, `bin/livelily_desktop`): LiveLily in a JUCE window, drawn with `JuceCanvas` every frame of a timer, the editor's grids painted by `CanvasGrid` (`grid_painter.*`, in the core, a port of `of_grid_painter` to the canvas), the keys with the codes of `keys.h`, the system's clipboard and file dialogs, and full screen without a title bar; checked live
- [x] Step 9: test mode in the JUCE app, its screenshots compared with those of the openFrameworks app; checked live
  - `livelily_desktop --test file.lyv [--out dump.txt] [--screenshot image.png]`, drawn offscreen with no window, frame by frame as the openFrameworks app
    runs a test; `LIVELILY_APP=livelily_desktop tests/snapshots.sh check` and `LIVELILY_APP=livelily_desktop tests/screenshots.sh save name` run it,
    and `tests/screenshots.sh side a b` puts two sets of screenshots side by side
  - 62 of the 63 snapshots of the openFrameworks app are the same with the JUCE app; `font_size_panes_test` differs in the rows of the panes,
    as the editor's font is measured by the outlines of its glyphs, where openFrameworks measured FreeType's hinted bitmaps:
    the cells are the same at size 18 (11x22), and at other sizes a pixel or two taller or shorter
    (its snapshot is saved anew with the JUCE app when openFrameworks goes, in step 10)
  - the screenshots differ only in the antialiasing of text and shapes; the screenshots of the openFrameworks app have the alpha of the window,
    so a translucent note of the piano roll looks lighter in an image viewer, though its color is the same
  - the desktop app's window goes on the screen once it has its size and its place (it showed at 128x128 in a corner for a moment)
- [ ] Step 10: openFrameworks is removed, and CI builds the desktop app

## What depends on openFrameworks today

| Files | What | Goes to |
| --- | --- | --- |
| `ofApp.cpp`, `ofApp.h`, `alignment.cpp`, `testmode.cpp` | the window, the drawing, and the front end's logic (panes, shell area, key dispatch, alignment, note highlights, quit prompt, test mode) | the logic to the core (steps 4 to 6), the rest to the JUCE app (step 8) |
| `score.cpp`, `score.h`, `of_instrument_display.*` (now `score_view.*`) | the engraved score, with Adobe's Sonata font | a drawing interface (step 7), then JUCE and Bravura (step 8) |
| `of_grid_painter.*`, `of_color.h` | painting the editor's grids | a JUCE painter (step 8) |
| `of_midi.*` (ofxMidi) | MIDI output | JUCE (step 2) |
| `of_osc.cpp` (ofxOsc) | OSC | in-house (step 1) |
| `of_audio.cpp` (ofSoundStream) | audio for pyo and the click track | JUCE (step 2) |
| `of_serial.cpp` (ofSerial) | serial ports | in-house (step 3) |
| `main.cpp` | the window and the command line | the JUCE app (step 8) |

## Why OSC is in-house

JUCE's OSC module (`juce_osc`) knows only int32, float32, string, blob and colour arguments, and it drops a whole message with any other type.
LiveLily sends booleans to LiveLily-part (`T` and `F`), and reads int64, double, symbol and boolean arguments from other programs.
The packets of OSC are simple to write and read, and UDP sockets are a few system calls on each system, so both are written in-house,
which also lets the openFrameworks app use them before JUCE is in.
Like ofxOsc, the senders may broadcast, the receivers share their port with other programs (`SO_REUSEADDR`, `SO_REUSEPORT`),
and bundles are unpacked into their messages.

## What JUCE changes for MIDI and audio

- **MIDI on Linux is in-house** (`platform/alsa_midi.cpp`, with the ALSA sequencer). JUCE 8 sends MIDI 2.0 events (UMP) whenever
  the ALSA library knows them, but a kernel whose sequencer is built without UMP (`CONFIG_SND_SEQ_UMP`, e.g. Debian 13's) drops them,
  so nothing arrives, though the ports connect. LiveLily sends MIDI 1.0 events instead, which every kernel delivers, lists the ports
  in ALSA's order and names them as RtMidi did (e.g. `Pure Data:Pure Data Midi-In 1 130:0`), so `\inst.midiport` takes the numbers
  it takes in the openFrameworks app. It opens a port of its own (`LiveLily out N`) for every port it sends to, as RtMidi did.
- **On macOS and Windows, MIDI is JUCE's,** whose ports are found through its message loop (since JUCE 8 the ports are listed
  and opened on the message thread). The JUCE app runs the loop anyway; the headless runner, which has no app, runs it for a moment
  before `--play` (`PlatformScope::findDevices()`, in `platform/`). The ports are numbered in JUCE's order.
- **Audio devices of every type are listed together** (`\listaudiodev`), each by its type and its name (e.g. `ALSA: ...`, `JACK: ...`).
  With no device set, the stream opens the default device of the first type (ALSA on Linux, CoreAudio on macOS, WASAPI on Windows),
  or JACK (below). `livelily_headless --play` writes the device it opened to the error stream (`audio: ...`).
  JUCE has no number of buffers, so `AudioSettings::numBuffers` is unused.
- **No OpenGL in the desktop app.** An OpenGL context delayed the keys by seconds on Linux (under XWayland): its thread draws through
  the X connection of the window, and reads the window's events, which then wait in Xlib's queue until something else wakes the app.
  LiveLily draws a frame in well under a millisecond with JUCE's own renderers (software on Linux, CoreGraphics on macOS,
  Direct2D on Windows), so the app has no OpenGL context and no `juce_opengl`.
- **The keys of the desktop app, on Linux.** JUCE gives no release of a key, only a change of the keys that are down, and the automatic
  repeat of a held key releases and presses it at once, so the keys that are no longer down are released once a frame. The modifiers are
  checked on every key and whenever the window gets or loses the keys. While LiveLily handles a key, what JUCE tells about the keys
  waits until the next frame (a file dialog or full screen takes the keys away in the middle of a key). Full screen changes the
  title bar, which makes the window anew, and a file dialog is another program (zenity or kdialog), so afterwards the window is
  brought to the front until it has the keys again.
- **JACK** is enabled on Linux (`JUCE_JACK`): JUCE loads the JACK library when the program runs. With no device set, the stream opens
  JACK if a JACK server runs (which usually holds the sound card, so that ALSA's default device, PipeWire or PulseAudio, can't play),
  and otherwise the default device of the first type. The JACK library would print that it can't connect whenever JUCE looks
  for a server and none runs, so LiveLily gives it functions for its errors and notes that print nothing (see `juce_audio.cpp`).

## Serial ports, compared with ofSerial

- On Linux the devices are listed `ttyACM*`, `ttyUSB*`, `rfcomm*`, then `ttyS*` (the motherboard's, often 32), with the numbers
  of their names in numerical order, so that a board on USB is usually port 0. macOS lists `cu.*` and `tty.*`, Arduinos first, as ofSerial did.
- Windows lists the COM ports of the registry (`HARDWARE\DEVICEMAP\SERIALCOMM`), which has the boards on USB that ofSerial's
  enumeration missed; a write waits at most a second.
- No flow control: ofSerial turned on RTS/CTS on Linux, which blocks writes to a device without those lines.
- 460800 and 921600 baud are added; any other speed LiveLily doesn't know is 9600, as with ofSerial.

## Follow-ups

- `\insts.clear` stops the sequencer (`stopNow()`) but doesn't wait for its thread to end before clearing the instruments,
  as Ctrl+n now does (see `Workspace::newSession()`); no crash has been seen, but it is the same kind of race.

## Steps, in order

1. **OSC in-house.** `osc_packet.cpp` writes and reads OSC packets (every type LiveLily reads, bundles included), `udp.cpp` has UDP sockets
   (POSIX and Winsock), and `osc_udp.cpp` makes the senders and receivers of `osc.h` with them. The receivers don't need a thread:
   the app reads them in every frame, and a non-blocking socket gives what has arrived. `of_osc.cpp` and the ofxOsc addon go,
   the headless runner uses the same OSC, and a CTest test checks packets and a round trip through a socket on the three systems.
2. **JUCE in CMake, MIDI and audio with JUCE.** JUCE from `LIVELILY_JUCE_DIR`, or fetched at a fixed version (8.0.12). A platform library
   with JUCE's audio devices and MIDI output (the ALSA sequencer's on Linux, see above), which the headless runner uses for `--play`
   (test mode keeps its made-up MIDI ports and an audio stream that never plays).
   RtMidi goes. The app keeps ofxMidi and ofSoundStream until step 10, as JUCE and openFrameworks don't go in one program.
3. **Serial ports in-house.** Listing the devices and writing to them with termios on Linux and macOS and the Win32 API on Windows,
   in the app (in place of ofSerial) and in the headless runner.
4. **The panes and the shell area leave `ofApp`.** A class of the core with the editors, the layout of the panes and the shell area,
   which `ofApp` draws. The snapshots don't change.
5. **The key dispatch and the quit prompt leave `ofApp`.** `ofApp::executeKeyPressed()` and the keys of the quit prompt go to the same class,
   so that a front end only sends key events to it.
6. **Alignment, the note highlights and test mode leave `ofApp`.** Then the headless runner has the editors too, and the headless tests
   compare the editor's sections of the dumps as well (and stage 5 needs nothing more from `ofApp`).
7. **The views draw through an interface.** Lines, rectangles, paths, text and glyphs, painted by openFrameworks; the screenshots don't change.
8. **The JUCE desktop app.** A window with the grid painter, the keys of `keys.h` (key releases rebuilt from JUCE's modifier callbacks and key states),
   the clipboard, the file dialogs, the mouse and the frame rate, an OpenGL context (and how fast the views draw, measured early),
   and the score with Bravura in place of Sonata.
9. **Test mode in the JUCE app.** The snapshots of the JUCE app are the same as those of the openFrameworks app,
   and their screenshots are compared side by side.
10. **openFrameworks goes.** The makefiles, the `of_*` files, `config.make` and the addons; CMake builds the app, and CI builds it on the three systems.
