# Stage 3 inventory: CMake and continuous integration

Stage 3 of the migration plan builds LiveLily with CMake and sets up continuous integration (CI) that builds it on Linux, macOS and Windows and runs the tests with no window.
Each step is one commit, checked with the snapshot tests of the app and the headless tests.

## Progress

- [x] Step 1: portable paths in the core (`homeDirectory()` in `system_command.h`, absolute paths with `std::filesystem`, and the headless runner finds itself on Linux, macOS and Windows, or takes `--data`)
- [x] Step 2: MIDI ports of their own in test mode (`test_midi.cpp`: two made-up ports, in the app and in the headless runner)
- [x] Step 3: CMake for the core and the headless runner (`CMakeLists.txt`: `livelily_core`, `livelily_headless` into `bin/`, FluidSynth with pkg-config or a CMake package, RtMidi 6.0.0 fetched or from `LIVELILY_RTMIDI_DIR`, and `LIVELILY_PYO`); `headless/build.sh` is gone, and `tests/headless.sh` builds with CMake
- [ ] Step 4: the tests in CTest
- [ ] Step 5: CI on Linux, macOS and Windows

## What is built today

- **The app** (`bin/livelily`) is built by openFrameworks' makefiles (`make wpyo`), on Linux only so far. It stays that way until stage 4 replaces openFrameworks with JUCE, which is built with CMake: building openFrameworks itself with CMake would be work thrown away in stage 4.
- **The headless runner** (`bin/livelily_headless`) is built by `headless/build.sh` from the engine's sources, with no openFrameworks. It takes RtMidi from the ofxMidi addon of openFrameworks, and `ofxMidiConstants.h` is replaced by an empty header for it.

So CMake builds what doesn't need openFrameworks: the core (engine, editor, grids, keys) and the headless runner. That is also what CI can build and test on the three systems.

## The core, file by file

| Files | What | Needs |
| --- | --- | --- |
| `session.cpp`, `session_setup.cpp`, `parser.cpp`, `sequencer.cpp`, `instrument.cpp`, `function.cpp`, `clock.cpp`, `osc.cpp`, `codetext.cpp`, `system_command.cpp`, `session_dump.cpp` | the engine | standard C++ |
| `soundfont.cpp` | SoundFonts (`\instname.sndfont`) | FluidSynth |
| `PyoClass.cpp`, `pyo_classes.cpp` | Python and pyo (`USEPYO`) | Python, with pyo installed |
| `editor.cpp`, `cellgrid.cpp`, `keys.cpp` | the editor, its grids, keys | standard C++ |
| `headless/main.cpp`, `headless/platform.cpp` | the headless runner and its platform | RtMidi (ALSA on Linux, CoreMIDI on macOS, WinMM on Windows) |

The core compiles without `USEPYO` too. Only one test file records what pyo does (`dataset1`), so a build without pyo skips it.

## What isn't portable yet

- `parser.cpp` expands `~` in paths with `getenv("HOME")`, which is unset on Windows (and the program crashes on a null pointer if it's unset anywhere).
- `headless/main.cpp` finds the data directory from `/proc/self/exe`, which exists on Linux only.
- `system_command.cpp` already uses `_popen()` on Windows.

## What depends on the machine

- **MIDI ports.** Test files that open a port by its number (`\inst.midiport 3` in `dur_percentage_test`, `\openmidiport 0` in `midiclock_test`) get a different result on a machine with a different number of ports, e.g. with a MIDI device plugged in, or on CI with none. Test mode uses a fixed list of two made-up ports instead, in the app and in the headless runner, which gives the results the snapshots have now.
- **Serial ports and audio devices.** Their lists are not written to the dumps already, and the test files only give them wrong arguments, which give the same errors everywhere.

## Dependencies on CI

- **FluidSynth:** `libfluidsynth-dev` (apt) on Linux, `fluid-synth` (Homebrew) on macOS, `fluidsynth` (vcpkg) on Windows.
- **RtMidi:** fetched by CMake at a fixed version (MIT), or found on the system. The ofxMidi copy needs openFrameworks.
- **Python and pyo:** optional (`LIVELILY_PYO`). On Linux, CI can install pyo with pip. On macOS and Windows it builds without pyo at first, and the plan's open question of bundling Python decides the rest (stage 6).
- **A hosted repository.** CI runs where the repository is hosted (e.g. GitHub Actions), and LiveLily has no remote yet. The workflow file is written and its steps checked locally; it runs once the repository is pushed.

## Steps, in order

1. **Portable paths in the core.** A helper for the home directory (`HOME`, or `USERPROFILE` on Windows, and no crash when neither is set), and the headless runner finds its data directory from its own path portably (or from a `--data` option).
2. **MIDI ports of their own in test mode.** Test mode, in the app and in the headless runner, lists two made-up MIDI ports and opens them without sending anything, so the dumps don't depend on the machine. The snapshots stay the same.
3. **CMake for the core and the headless runner.** `CMakeLists.txt` at the root: a `livelily_core` library, the `livelily_headless` executable, FluidSynth found with pkg-config or CMake's packages, RtMidi fetched or found, and the option `LIVELILY_PYO`. `headless/build.sh` is replaced by it. The app keeps `make wpyo`.
4. **The tests in CTest.** Every test file with a snapshot is a CTest test that runs the headless runner and compares its dump with the snapshot, up to the editor's sections (as `tests/headless.sh` does), with a CMake script, so that it runs on Windows too. Tests that need pyo are skipped without it.
5. **CI on Linux, macOS and Windows.** A GitHub Actions workflow that installs the dependencies, configures, builds and runs CTest on the three systems. This ends stage 3, once the repository is pushed.

The app's own test mode (with the editor's sections and the screen) keeps running locally with `tests/snapshots.sh`, as it needs openFrameworks and a window. The editor's sections come to the headless tests when the panes leave `ofApp` (stage 5).
