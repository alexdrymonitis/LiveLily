#pragma once

// the state of the view of the openFrameworks front end: the size of the window, the positions of the staffs,
// the piano roll, the beat visualization, the area below the panes and the text typed from Python into a pane
// the engine (Session, the sequencer) doesn't see it, and reaches it only through FrontendHost (frontend.h)

#include <cstdint>
#include <map>
#include <string>
#include <vector>

// size of cosine table to control the transparency of the beat visualization rectangle
#ifndef BEATVIZBRIGHTNESS
#define BEATVIZBRIGHTNESS 255
#endif

struct pyStdoutStrKeyModifier
{
	bool isModifier;
	int ndxInc;
	int modifier;
	int action;
};

struct ViewState
{
	// handle strings typed from a Python pane to a LiveLily pane
	std::string pyStdoutStr;
	size_t pyStdoutStrNdx;
	bool typePyStrCharByChar;
	bool typePyStrLineByLine;
	int whichPyPane;
	bool typePyStr;
	std::vector<std::string> pyStdoutStrVec;
	int pyStdoutStrVecNdx;
	std::vector<pyStdoutStrKeyModifier> pyStdoutKeyModifiers;
	int pyStdoutKeyModNdx;
	std::vector<int> pyStdoutKeyModNdxs;

	float notesXStartPnt;

	int noteWidth;
	int noteHeight;
	// whether the score is shown (the engine asks with FrontendHost::scoreShown())
	bool showNotes;
	// the piano roll (see \score.show pianoroll), whose time stamp is the start of the bar that plays (see FrontendHost::barStarted())
	int pianoRollNumWhiteKeys;
	// the length of the keyboard of the piano roll in widths of white keys, from the start of its lowest key to the end of its highest key
	// (a black key at either end ends at its side, see ofApp::setPianoRollRange()), and where it starts, from C0, in widths of white keys
	float pianoRollLength = 52;
	float pianoRollOrigin = 0;
	// the lowest and highest keys of the piano roll, as MIDI notes, A0 to C8 like a piano by default (see \score.show pianoroll)
	int pianoRollLowestKey = 21;
	int pianoRollHighestKey = 108;
	float pianoRollKeysWidth;
	float pianoRollKeysHeight;
	uint64_t pianoRollTimeStamp;
	// the beat visualization, set on every beat by FrontendHost::beat()
	// setBeatAnimation waits for the sequencer to start (see FrontendHost::sequencerStarted())
	bool setBeatAnimation;
	bool beatViz;
	float beatVizStepsPerMs;
	uint64_t beatVizRampStart;
	uint64_t beatVizTimeStamp;
	int beatVizDegrade;
	int beatVizCosTab[BEATVIZBRIGHTNESS+1];
	bool beatAnimate;
	bool beatTypeCommand;
	int beatVizType;
	bool beatUpdated;
	// the bars shown in horizontal view, and the number and position of the previous ones, kept when the loop changes (see FrontendHost::loopChanged())
	int numBars;
	int thisPosition;
	int prevNumBars;
	int prevPosition;
	float maxBarFirstStaffAnchor;
	float allStaffDiffs;
	float yStartPnts[3];

	float staffLinesDist;
	int longestInstNameWidth;
	// variable to leave some space between edge of score and
	// instrument name, instrument name and staff, etc.
	int blankSpace;

	int scoreFontSize;

	// the window
	int screenWidth;
	int screenHeight;
	int middleOfScreenX;
	int middleOfScreenY;
	int staffXOffset;
	int staffWidth;
};
