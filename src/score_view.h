#ifndef SCORE_VIEW_H
#define SCORE_VIEW_H

#include "instrument.h"
#include "score.h"

// what is drawn of an instrument in the score: its staff and its notes (see score.h), which the front end attaches to the instrument
// and which draws with the canvas of the front end (see canvas.h)
// (see InstrumentDisplay in instrument.h, and Views::instrumentCreated()), and reaches with instrument.displayAs<ScoreView>()
class ScoreView : public InstrumentDisplay
{
	public:
		ScoreView();
		// what the engine tells it (see InstrumentDisplay)
		void setID(int id) override { staff.setID(id); notesObj.setID(id); }
		void setGroup(int groupID) override { staff.setGroup(groupID); }
		void setRhythm(bool isRhythm) override { staff.setRhythm(isRhythm); notesObj.setRhythm(isRhythm); }
		void setActiveNote(int ndx) override { notesObj.setActiveNote(ndx); }
		void setClef(int bar, int clefIdx) override { staff.setClef(bar, clefIdx); notesObj.setClef(bar, clefIdx); }
		void deleteBar(int bar) override { notesObj.deleteBar(bar); }
		void setMeter(int bar, int numerator, int denominator, int numBeats) override { staff.setMeter(bar, numerator, denominator); notesObj.setMeter(bar, numerator, denominator, numBeats); }
		void setAccidentalsOffsetCoef(float coef) override { notesObj.setAccidentalsOffsetCoef(coef); }
		void setAnimation(bool animationState) override { notesObj.setAnimation(animationState); }
		void setLoopStart(bool loopStartState) override { staff.isLoopStart = loopStartState; }
		void setLoopEnd(bool loopEndState) override { staff.isLoopEnd = loopEndState; }
		void setScoreEnd(bool scoreEndState) override { staff.isScoreEnd = scoreEndState; }
		void setMute(bool mute) override { notesObj.setMute(mute); }
		void setVoice(int voice, int numVoices) override { notesObj.setVoice(voice, numVoices); }

		// the layout and the drawing of the score, which only the front end does
		// the notes of a bar, from the data of the instrument the parser has made (e.g. scoreNotes)
		void setNumBarsToDisplay(int numBars);
		float getXCoef();
		std::pair<int, int> isLinked(int bar);
		std::pair<int, int> getMeter(int bar);
		void setScoreNotes(Instrument& inst, int bar, int numerator, int denominator, int numBeats, int BPMTempo, int beatAtValue, bool hasDot, int BPMMultiplier);
		void setNotePositions(int bar);
		void setNotePositions(int bar, int numBars);
		void setNoteCoords(float xLen, float staffLineDist, int fontSize);
		void setScoreOrientation(int orientation);
		void setStaffColor(Color color);
		void setStaffColor(int color, int val);
		void setStaffColor(int r, int g, int b);
		Color getStaffColor();
		void setNotesColor(Color color);
		void setNotesColor(int r, int g, int b);
		void setNotesColor(int color, int val);
		Color getNotesColor();
		void setActiveNotesColor(Color color);
		void setActiveNotesColor(int color, int val);
		void setActiveNotesColor(int r, int g, int b);
		Color getActiveNotesColor();
		void invertColor();
		void moveScoreX(int numPixels);
		void moveScoreY(int numPixels);
		void recenterScore();
		float getStaffXStartOffset();
		float getStaffXLength();
		float getNoteWidth();
		float getNoteHeight();
		void setStaffCoords(float xStartPnt, float staffLineDist);
		void setNotesFontSize(int fontSize, float staffLinesDist);
		float getMinVsAnchor(int bar);
		float getMaxYPos(int bar);
		float getMinYPos(int bar);
		float getClefXOffset();
		float getMeterXOffset();
		// barOfLoop and loopLength place the bar in its loop, for its repeat signs in horizontal view (see Staff::drawStaff())
		void drawStaff(int bar, float xOffset, float yStartPnt, float yOffset, bool drawClef, bool drawMeter, bool drawLoopStartEnd, bool drawTempo,
				int barOfLoop = -1, int loopLength = 0);
		void drawNotes(int bar, int loopNdx, std::vector<int> *v, float xOffset, float yStartPnt, float yOffset, bool animate, float xCoef, int nextBar = -1);
		int getForcedStemDir() { return notesObj.getForcedStemDir(); }
		std::vector<Notes::NoteColumn> getNoteColumns(int bar) { return notesObj.getColumns(bar); }
		void shiftNoteColumn(int bar, int noteNdx, float dx) { notesObj.shiftColumn(bar, noteNdx, dx); }
		float getScoreNoteWidth() { return notesObj.getNoteWidth(); }

	private:
		Staff staff;
		Notes notesObj;
};

#endif
