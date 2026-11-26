// test mode, for checking that changes to the parser don't change what LiveLily code does
// livelily --test file.lyv [--out dump.txt] (see main.cpp) runs the file with a hidden window, chunk by chunk
// as if every definition and every line were executed in turn in the editor
// and writes the errors, warnings, and notes of the lines, and the parsed state, to a text file
// if the file starts the sequencer (e.g. with \score.play), the sequencer plays a dry run at the end, on a simulated clock
// and what it would have sent (MIDI, OSC, pyo code, clicks, bound functions) is written with its time (see Sequencer::dryRun())
// then the keys of the lines "% test keys: ..." are typed (see keys.h), and a few frames after that, when the window has been drawn
// the state is written, and the screen: the grids of the panes and of the area below them (see cellgrid.h)
// the dumps of the example files are compared before and after a change to the parser (see tests/snapshots.sh)

#include "ofApp.h"
#include "test_run.h"
#include <fstream>
#include <iostream>

//--------------------------------------------------------------
void ofApp::runTest()
{
	std::ofstream out(testOut);
	if (!out.is_open()) {
		std::cerr << "livelily test: could not write to " << testOut << std::endl;
		return;
	}
	testKeyNdx = 0;
	TestRun::runFile(app.workspace, app.session, testFile, out, testKeys);
}

//--------------------------------------------------------------
void ofApp::appendTestEnd()
{
	std::ofstream out(testOut, std::ios::app);
	if (!out.is_open()) return;
	TestRun::writeState(app.workspace, app.session, out, ofGetTargetFrameRate());
	TestRun::writeScreen(app.workspace, out);
}
