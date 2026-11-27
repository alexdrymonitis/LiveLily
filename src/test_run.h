#ifndef TEST_RUN_H
#define TEST_RUN_H

#include "keys.h"
#include <ostream>
#include <string>
#include <vector>

class Session;
class Workspace;

// test mode, for checking that changes don't change what LiveLily does, in the app and in the headless runner alike
// a test file runs in the active pane, chunk by chunk, as if every definition and every line were executed in turn in the editor
// and its dump has the errors, warnings, and notes of the lines, and if the file starts the sequencer, what a dry run of it sends
// (see Sequencer::dryRun()), then, after the keys of the lines "% test keys: ..." have been typed (see keys.h), the state:
// the session, the tokens the notes highlight, and the text of the editor, and the screen (the grids of the panes and of the area below them)
// the dumps are compared with snapshots (see tests/snapshots.sh, and tests/compare.cmake for the headless runner)
namespace TestRun {

// loads the file into the active pane and runs it, writing the messages of its lines and the playback
// and gives the keys it types; returns false if the file can't be loaded (which is written to the dump)
bool runFile(Workspace& workspace, Session& session, const std::string& file, std::ostream& out, std::vector<KeyEvent>& keys);

// the state after the keys have been typed, with the frame rate the front end has set (\framerate)
void writeState(Workspace& workspace, Session& session, std::ostream& out, float frameRate);

// the screen: every pane (its text, line numbers, and status line) and the area below the panes
void writeScreen(Workspace& workspace, std::ostream& out);

}

#endif
