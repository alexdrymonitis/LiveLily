#ifndef INSTRUMENT_H
#define INSTRUMENT_H

#include "color.h"
#include "osc.h"
#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#define MINDUR 256

// what the front end draws of an instrument (its staff and its notes, see score_view.h), as the engine sees it
// the engine tells it what changes, as these calls, and the front end does the rest (layout, colors, drawing)
class InstrumentDisplay
{
	public:
		virtual ~InstrumentDisplay() = default;
		virtual void setID(int id) = 0;
		virtual void setGroup(int groupID) = 0;
		virtual void setRhythm(bool isRhythm) = 0;
		// the note that plays, which is drawn in the color of the active notes
		virtual void setActiveNote(int ndx) = 0;
		virtual void setClef(int bar, int clefIdx) = 0;
		virtual void deleteBar(int bar) = 0;
		virtual void setMeter(int bar, int numerator, int denominator, int numBeats) = 0;
		virtual void setAccidentalsOffsetCoef(float coef) = 0;
		virtual void setAnimation(bool animationState) = 0;
		virtual void setLoopStart(bool loopStartState) = 0;
		virtual void setLoopEnd(bool loopEndState) = 0;
		virtual void setScoreEnd(bool scoreEndState) = 0;
		virtual void setMute(bool mute) = 0;
		virtual void setVoice(int voice, int numVoices) = 0;
};

class Instrument
{
	public:
		Instrument();
		void setName(std::string name);
		std::string getName();
		void setID(int id);
		int getID();
		void setColor(Color color);
		void setGroup(int groupID);
		Color getColor();
		int getGroup();
		void setRhythm(bool isRhythm);
		bool isRhythm();
		void setSendToPython(bool b);
		bool sendToPython();
		void setDelay(int64_t dur);
		bool hasDelay();
		int64_t getDelayTime();
		void setSendMIDI(bool sendMIDI);
		bool sendMIDI();
		void setTransposition(int transpo);
		int getTransposition();
		void setDefaultDur(float dur);
		void setStaccatoDur(float dur);
		void setStaccatissimoDur(float dur);
		void setTenutoDur(float dur);
		int setDuration(std::string subcommand, float dur);
		void setPassed(bool passed);
		bool hasPassed();
		void setCopied(int barIndex, bool copyState);
		bool getCopied(int barIndex);
		void setCopyNdx(int barIndex, int barToCopy);
		int getCopyNdx(int barIndex);
		bool hasNewStep();
		void setNewStep(bool stepState);
		int getNewStep();
		bool getMultiBarsDone();
		void setMultiBarsDone(bool done);
		size_t getMultiBarsStrBegin();
		void setMultiBarsStrBegin(size_t pos);
		void setClef(int bar, int clefIdx);
		int getClef(int bar);
		void createEmptyMelody(int barIndex);
		void deleteNotesBar(int bar);
		void setMeter(int bar, int numerator, int denominator, int numBeats);
		void setAccidentalsOffsetCoef(float coef);
		void setGlissandoStart(bool startGliss);
		bool getGlissandoStart();
		float getStaffYAnchor();
		void setAnimation(bool animationState);
		void setLoopStart(bool loopStartState);
		void setLoopEnd(bool loopEndState);
		void setScoreEnd(bool scoreEndState);
		bool isLoopStart();
		bool isLoopEnd();
		void initSeqToggle();
		void setFirstIter(bool iter);
		void setToMute(bool mute);
		bool isToBeMuted();
		void setToUnmute(bool unmute);
		bool isToBeUnmuted();
		void setMute(bool mute);
		bool isMuted();
		void setMidi(bool isMidiBool);
		bool isMidi();
		void setTimeStamp(uint64_t stamp);
		void resetNoteDur();
		void zeroNoteDur();
		bool mustFireStep(uint64_t stamp, int bar, float tempo);
		void setNoteDur(int bar, float tempo);
		int getDurPercentage(int bar);
		bool isNoteSlurred(int bar, int dataCounter);
		bool isNoteTied(int bar, int dataCounter);
		bool isLastNoteTied(int bar);
		bool mustUpdateTempo();
		void setUpdateTempo(bool tempoUpdate);
		bool hasNotesInBar(int bar);
		bool hasNotesInStep(int bar);
		void setMidiPort(int port);
		int getMidiPort();
		void setMidiChan(int chan);
		int getMidiChan();
		// the channel of the SoundFont synthesizer the instrument plays on, -1 if it doesn't have a SoundFont (see soundfont.h)
		void setSoundFontChannel(int chan);
		int getSoundFontChannel();
		void setActiveNote();
		void incrementBarDataCounter(int bar);
		int getBarDataCounter();
		void resetBarDataCounter();
		void toggleSeqToggle(int bar);
		int getSeqToggle();
		void storeTempBarDataCounter();
		bool hasText(int bar);
		std::string getText(int bar);
		int getPitchBendVal(int bar);
		int getProgramChangeVal(int bar);
		int getMidiVel(int bar);
		int getArticulationIndex(int bar);
		float getDynamic(int bar);
		// the voice of the instrument in a staff with merged instruments (see \merge and Notes::setVoice() in score.h)
		void setVoice(int voice, int numVoices) { this->voice = voice; this->numVoices = numVoices; voiceSet = true; if (display) display->setVoice(voice, numVoices); }
		void printVector(std::vector<int> v);
		void printVector(std::vector<std::string> v);
		void printVector(std::vector<float> v);

		std::string name;
		std::map<int, float> distBetweenBeats;
		// maps of int and map of int and vector (of vectors)
		// int key of outter map is instrument index, retrieved from instrumentIndexes above
		// int key of inner map is bar index, retrieved from barsIndexes above
		// vector if bar data
		// if vector of vectors, it is chord notes
		std::map<int, std::vector<std::vector<float>>> notes;
		std::map<int, std::vector<std::vector<int>>> midiNotes;
		std::map<int, std::vector<int>> durs;
		std::map<int, std::vector<int>> dursWithoutSlurs;
		std::map<int, std::vector<int>> midiDursWithoutSlurs;
		// the pitch bend of every note (for MIDI and SoundFonts), for quarter tones, from -8192 to 8191 over 12 semitones up and down
		std::map<int, std::vector<int>> pitchBendVals;
		// the last pitch bend sent for quarter tones, or INT_MIN when a glissando has changed it, so that it is sent again
		int lastPitchBendSent = 0;
		// vectors not affected by slurs and ties, needed for counting in the sequencer
		std::map<int, std::vector<int>> dursUnchanged;
		std::map<int, std::vector<float>> dynamics;
		std::map<int, std::vector<int>> midiVels;
		std::map<int, std::vector<float>> dynamicsRamps;
		std::map<int, std::vector<int>> midiDynamicsRampDurs;
		std::map<int, std::vector<int>> glissandi;
		// the start note and the direction (1 for crescendo, 0 for diminuendo) of a crescendo or diminuendo left open at the end of a bar
		// (-1 if there is none), which goes on to the first note of the next bar (see Sequencer::noteDynamics())
		std::map<int, std::pair<int, int>> openDynamicsRamp;
		// the crescendo or diminuendo the sequencer plays during the current note, from its dynamic to the dynamic of the next note
		// (see Sequencer::startDynamicsRamp()), with the dynamics and MIDI velocities of its start and end
		bool dynamicsRampActive = false;
		std::pair<float, int> dynamicsRampFrom;
		std::pair<float, int> dynamicsRampTo;
		int64_t dynamicsRampStartMs = 0;
		int64_t dynamicsRampDurMs = 1;
		int64_t dynamicsRampLastMs = 0;
		// MIDI notes in a crescendo or diminuendo start with the velocity of its louder end
		// and the expression controller (CC 11) changes their level (127 is the full velocity)
		int dynamicsRampVel = 127;
		int lastExpressionSent = 127;
		// false until the sequencer sets the time of the instrument (see Sequencer::threadedFunction())
		// an instrument added while the sequencer plays (with \insts.add) starts playing at the start of the next bar
		bool timeSynced = false;
		// an instrument with a delay (\inst.delay) plays ahead of the score by its delay, for instruments that sound late
		// (e.g. a Disklavier), so the sequencer plays it on its own (see Sequencer::playDelayedInstrument())
		// with the position in its loop, its bar, the time it started that bar, and the bar before it, which is -1 when it starts
		bool soundSynced = false;
		int soundLoopIndex = 0;
		int soundBarIndex = 0;
		int soundBar = 0;
		int soundPrevBar = -1;
		int64_t soundBarStart = 0;
		bool soundFirstStep = true;
		// \mute and \unmute apply to the sound of such an instrument when it starts the next bar, before the score does
		// -1 when its sound follows isMuted()
		int soundMuteState = -1;
		// sets the step the sequencer plays next, and the time it plays it
		void setSeqStep(int step, int64_t stamp);
		// the time from the start of a bar to the start of a step, in microseconds
		int64_t stepStartUs(int bar, int step, float tempo);
		void setActiveNote(int ndx);
		std::map<int, std::vector<uint64_t>> glissDursMs;
		std::map<int, std::vector<int>> midiGlissDurs;
		std::map<int, std::vector<std::vector<int>>> articulations;
		std::map<int, std::vector<std::vector<int>>> midiArticulationVals;
		std::map<int, std::vector<bool>> isSlurred;
		std::map<int, std::vector<bool>> isCluster;
		std::map<int, std::vector<std::string>> text;
		std::map<int, std::vector<std::vector<int>>> textIndexes;
		std::map<int, std::vector<std::pair<int, int>>> slurIndexes;
		std::map<int, std::vector<int>> tieIndexes;
		// same std::vectors for sending data to the Notes objects
		std::map<int, std::vector<std::vector<int>>> scoreNotes; // notes are ints here
		std::map<int, std::vector<int>> scoreDurs;
		std::map<int, std::vector<int>> scoreDotIndexes;
		std::map<int, std::vector<unsigned>> scoreDotsCounter;
		std::map<int, std::vector<std::vector<int>>> scoreAccidentals;
		std::map<int, std::vector<std::vector<int>>> scoreNaturalSignsNotWritten;
		std::map<int, std::vector<std::vector<int>>> scoreOctaves;
		std::map<int, std::vector<int>> scoreOttavas;
		std::map<int, std::vector<int>> scoreGlissandi;
		std::map<int, std::vector<std::vector<int>>> scoreArticulations;
		std::map<int, std::vector<int>> scoreDynamics;
		std::map<int, std::vector<int>> scoreDynamicsIndexes;
		std::map<int, std::vector<int>> scoreDynamicsRampStart;
		std::map<int, std::vector<int>> scoreDynamicsRampEnd;
		std::map<int, std::vector<int>> scoreDynamicsRampDir;
		//std::map<int, std::vector<int>> scoreSlurBeginnings;
		//std::map<int, std::vector<int>> scoreSlurEndings;
		std::map<int, bool> isWholeBarSlurred;
		std::map<int, std::map<int, std::pair<int, int>>> scoreTupletRatios;
		std::map<int, std::map<int, std::pair<unsigned, unsigned>>> scoreTupletStartStop;
		std::map<int, std::vector<std::string>> scoreTexts;

		// variables for handling glissandi in the sequencer
		std::vector<float> glissStart;
		std::vector<float> glissEnd;
		std::vector<float> glissSteps;
		std::vector<int> glissNumSteps;
		// the MIDI note played for every note of the glissando, from which MIDI and SoundFont instruments bend it
		std::vector<int> glissMidiNotes;
		// the voice of every note of a MIDI glissando (0 for a single note), as the notes of a chord bend on channels of their own
		std::vector<int> glissVoices;
		// the voice of every MIDI note that is on, so that its note off goes to the same channel
		std::map<int, int> midiNoteVoices;
		// how many voices this instrument has used, to stop the sound of all of them
		int numMidiVoices = 1;
		int64_t glissTimeStamp;
		int64_t glissStartTimeStamp;
		int glissCounter;
		int64_t glissDurMs;
		int64_t glissTimeDiff;
		bool startGliss;

		// score parts OSC handling
		bool sendToPart;
		// the sender of the score part of the instrument on another computer (see \instname.sendto)
		std::shared_ptr<OscSender> scorePartSender = makeOscSender();
		// map to hold a boolean determining that a part that receives data has received bar data correctly
		std::map<int, bool> barDataOKFromParts; // keys are bar indexes
		// map of instrument index and string to store expanded bar lines to send to parts
		std::map<int, std::string> barLines;

		// the std::vector below is used in case a connected instrument needs a delay for incoming messages
		// so we store a pair with the OscMessage object and a time stamp
		std::vector<std::pair<OscMessage, uint64_t>> oscFifo;
		// the boolean below stores the state of the delay
		bool delayState;
		int64_t delayTime;

		// MIDI stuff
		int midiPort;
		int midiChan;
		bool isMidiBool;
		int soundFontChannel = -1;
		// duration percentage for various notes
		// size is 8 because of the number of available articulation symbols
		float durPercentages[8];
		float staccatoDur;
		float staccatissimoDur;
		float tenutoDur;

		uint64_t beatCounter;
		int barCounter;
		int barDataCounter;
		int tempBarDataCounter;
		bool barDataCounterReset;
		int seqToggle; // so we can alternate between notes on and notes off
		int64_t noteDur;
		uint64_t timeStamp;

		bool toMute;
		bool toUnmute;
		bool muteState;
		bool toMuteState;
		bool toUnmuteState;

		bool passed; // set to true when a melodic line passes all tests
		bool firstIter;

		// what the front end draws of the instrument (see InstrumentDisplay), which it attaches when the instrument is created
		// (see FrontendHost::instrumentCreated()), with what the instrument has been told so far
		// copies of the instrument (e.g. in the instruments map) share it
		std::shared_ptr<InstrumentDisplay> display;
		void attachDisplay(std::shared_ptr<InstrumentDisplay> newDisplay);
		// the display as the type of the front end, which is attached if there is none yet
		// a template, so that only the front end, which knows the type, uses it
		template <typename T> T& displayAs()
		{
			if (!display) attachDisplay(std::make_shared<T>());
			return *static_cast<T*>(display.get());
		}

	private:
		// what the instrument tells its display, kept so that a display attached later gets it too (see attachDisplay())
		// and so that the engine can read back what it set (getClef(), isLoopStart(), isLoopEnd())
		std::map<int, int> clefs;
		std::map<int, std::array<int, 3>> meters; // numerator, denominator, number of beats
		bool accidentalsOffsetSet = false;
		float accidentalsOffsetCoef = 0;
		bool animation = false;
		bool loopStart = false;
		bool loopEnd = false;
		bool scoreEnd = false;
		bool voiceSet = false;
		int voice = 1;
		int numVoices = 1;
		int objID;
		int groupID;
		bool multiBarsDone;
		size_t multiBarsStrBegin;
		bool hasNewStepBool;
		int newStep;
		bool updateTempo;
		bool isRhythmBool;
		bool sendMIDIBool;
		bool sendToPythonBool;
		int transposition;
		// the color of the instrument in the piano roll and the step sequencer
		Color color;

		std::map<int, bool> copyStates;
		std::map<int, int> copyNdxs;
};

#endif
