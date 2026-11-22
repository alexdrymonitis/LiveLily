#ifndef SEQUENCER_H
#define SEQUENCER_H

#include "ofMain.h"
#include "ofxOsc.h"
#include "ofxMidi.h"
#include "ofTimer.h"
#include <map>
#include <vector>
#include <string>
#include <functional>
#include "instrument.h"

// SharedData is defined in utils.h
// the sequencer only holds a pointer to it, so a declaration is enough here
// and sequencer.cpp includes utils.h for the full definition
struct SharedData;

class Sequencer : public ofThread
{
	public:
		void setup(SharedData *sData);
		void setSendMidiClock(bool sendMidiClockState);
		void start();
		void stop();
		void stopNow();
		void update();
		bool isUpdated();
		void setSequencerRunning(bool seqRun);
		void setFinish(bool finishState);
		void setCountdown(int num);
		void setMidiTune(float tuneVal);
		void setCheckLoop(std::string barLoopName, int numIter);

		ofxMidiOut midiOut;
		std::vector<ofxMidiOut> midiOuts;
		std::vector<std::string> midiOutPorts;
		std::map<int, int> midiPortsMap;

		// stops the sound of an instrument, before it is removed (see ofApp::removeInstrument())
		void silenceInstrument(Instrument& inst);
		// drops the highlights of instruments with a delay that haven't been shown yet (e.g. before an instrument is removed)
		void clearDelayedVisuals();
	private:
		/* instruments with a delay (\inst.delay), which play ahead of the score by their delay
		   the score, and the instruments without a delay, start after the longest delay, when \score.play is called
		   and the instrument with the longest delay starts first
		   a delayed instrument moves on to its next bar by itself, before the score does
		   and at the start of every bar of the score the sequencer checks that it is in the bar it should be in (see syncDelayedInstrument())
		   its highlights in the score and the editor, and the functions bound to it, are shown when its notes sound, after its delay */
		// the time the score starts, after the longest delay
		int64_t scoreStartTime = 0;
		// true while the sequencer plays an instrument with a delay (see playDelayedInstrument())
		bool aheadPass = false;
		// the position in the loops of the instrument played ahead, which nextBar() uses, -1 otherwise
		int aheadLoopIndex = -1;
		int aheadBarIndex = 0;
		// the highlights and function calls of instruments with a delay, with the time to show or call them
		std::multimap<int64_t, std::function<void()>> delayedVisuals;
		void runDelayedVisuals(int64_t now);
		// shows a highlight or calls a function now, or after the delay of the instrument, if it is played ahead
		void visual(Instrument& inst, std::function<void()> fn);
		void visualEditorElement(std::map<int, Instrument>::iterator instMapIt, int bar, int ndx, bool active);
		void visualEditorRest(std::map<int, Instrument>::iterator instMapIt, int bar, int ndx);
		void visualEditorClear(std::map<int, Instrument>::iterator instMapIt);
		void visualActiveNote(std::map<int, Instrument>::iterator instMapIt);
		void visualBoundFunction(std::map<int, Instrument>::iterator instMapIt, std::string funcName);
		// whether an instrument is muted, for its sound (see Instrument::soundMuteState)
		bool mutedForSound(Instrument& inst);
		// the duration of a bar in microseconds
		int64_t barDurUs(int bar);
		// moves a position in the loops to the next bar, as the sequencer plays them (with a loop that has been called, at the end of the loop)
		void advancePosition(int& loopNdx, int& barNdx);
		// sets the position of an instrument with a delay from the position of the score, with the score time of the start of its bar
		// if onlyIfChanged is true, it does so only if the instrument is not in the bar it should be in
		void syncDelayedInstrument(std::map<int, Instrument>::iterator instMapIt, int loopNdx, int barNdx, int64_t scoreBarStart, int64_t now, bool onlyIfChanged);
		void playDelayedInstrument(std::map<int, Instrument>::iterator instMapIt, int64_t now);
		int setBarIndex(bool increment);
		void checkMute();
		void sendAllNotesOff();
		// the MIDI messages of an instrument, which go to its SoundFont if it has loaded one, or else to its MIDI port
		void midiNoteOn(Instrument& inst, int note, int velocity, int voice = 0);
		void midiNoteOff(Instrument& inst, int note);
		void midiPitchBend(Instrument& inst, int value, int voice = 0);
		void midiProgramChange(Instrument& inst, int program);
		void midiAllSoundOff(Instrument& inst);
		void sendBeatVizInfo(int bar);
		float midiToFreq(float midiNote);
		float mapVal(float inVal, float fromLow, float fromHigh, float toLow, float toHigh);
		void startGlissando(int instNdx, int bar, int barDataCounter);
		void runGlissando(int instNdx, int bar);
		// sends the pitches of a glissando at a point from 0 (its start) to 1 (its end)
		void sendGlissando(int instNdx, float progress);
		// the MIDI channel of a voice of an instrument (see midiNoteOn())
		int midiVoiceChannel(Instrument& inst, int voice);
		// the bar that is played after a bar, and before it, as the sequencer plays them (in a loop, or when switching to another loop)
		// a bar that plays alone is played after itself
		int nextBar(int bar);
		int previousBar = -1; // -1 when the sequencer starts
		// a tie at the end of a bar connects its last note to the first note of the next bar, only if these are the same notes
		bool tiedAcrossBars(Instrument& inst, int fromBar, int toBar);
		// whether a note is tied to the note after it, which for the last note of a bar is the first note of the next bar
		bool noteTiedToNext(Instrument& inst, int bar, int noteNdx);
		// the dynamic and MIDI velocity of the current note, which in a crescendo or diminuendo left open at the end of the bar
		// go towards the dynamic of the first note of the next bar
		std::pair<float, int> noteDynamics(Instrument& inst, int bar);
		// the same for any note of a bar
		std::pair<float, int> noteDynamicsAt(Instrument& inst, int bar, int noteNdx);
		// the dynamic a crescendo or diminuendo left open at the end of a bar (starting at note start) reaches at the first note of the next bar
		std::pair<float, int> openRampEnd(Instrument& inst, int bar, int start);
		// whether a note is in a crescendo or diminuendo, and the dynamic it goes to during its duration (that of the next note)
		bool dynamicsRampTarget(Instrument& inst, int bar, int noteNdx, std::pair<float, int>& target);
		// starts a crescendo or diminuendo during the current note, from its dynamic to the target, which runDynamicsRamp() plays
		// for MIDI instruments, it returns the velocity of the note, and sends the expression controller it starts with
		int startDynamicsRamp(Instrument& inst, int bar, std::pair<float, int> from, std::pair<float, int> to);
		void runDynamicsRamp(int instNdx);
		// sends the dynamic of an instrument during a crescendo or diminuendo
		void sendDynamic(Instrument& inst, std::pair<float, int> dynamic);
		// sends the expression controller (CC 11) to all channels of a MIDI instrument
		void midiExpression(Instrument& inst, int value);
		void sendToParts(ofxOscMessage m, bool delay);
		void sendSequencerStateToParts(bool state);
		void sendLoopIndexToParts();
		void sendStopCountdownToParts();
		void sendCountdownToParts(int coundown);
		void sendFinishToParts(bool finishState);
		std::string setSeqOscAddress(std::string instName, std::string s, bool checking);
		void fireNote(std::map<int, Instrument>::iterator instMapIt, int bar, uint64_t timeStamp, bool checking);
		void threadedFunction();

		SharedData *sharedData;

		ofxOscSender oscSender;
		ofTimer timer;
		bool runSequencer;
		bool sequencerRunning;
		bool sequencerStart;
		bool updateSequencer;
		bool sequencerUpdated;
		bool updateTempo;
		bool finish;
		bool mustStop;
		bool mustStopCalled;
		int onNextStartMidiByte;
		uint64_t tickCounter;
		int beatCounter;
		int sendBeatVizInfoCounter;
		unsigned thisBarIndex;
		bool firstIter;
		bool endOfBar;
		int numBeats;
		bool finished;
		bool sendMidiClock;
		bool muteChecked;
		int countdownCounter;
		bool countdown;
		float midiTuneVal;
		int loopToCheckNdx;
		int loopToCheckBarNdx;
		int loopToCheckNumIter;
		int loopToCheckIterCounter;
		bool checkLoop;
		int tempBar;

		/* iterators for accessing data in Instrument objects */
		std::map<std::string, int>::iterator instNamesIt;
		//std::map<int, Instrument>::iterator instMapIt;

		/* and reverse iterators */
		std::map<std::string, int>::reverse_iterator instRevIt;
		std::map<int, Instrument>::reverse_iterator instMapRevIt;
};

#endif
