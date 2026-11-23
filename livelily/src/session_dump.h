#ifndef SESSION_DUMP_H
#define SESSION_DUMP_H

// the text of the test dumps (see testmode.cpp and headless/main.cpp), written the same way by both, so their snapshots can be compared

#include <ostream>
#include <string>

class Session;

// a string with its newlines and tabs written as \n and \t, so that it stays on one line of the dump
std::string escapedForDump(const std::string& str);
// a line of the "=== lines" section: the message (color 0 note, 1 warning, 2 error, as Editor::setTraceback() sets it) of a line of the file
// lineStr is the text of the line, as the notes of the commands that list the ports and devices of the machine are not written
void writeLineMessage(std::ostream& out, int lineNdx, const std::string& lineStr, int color, const std::string& text);
// the state of the session, from "=== bars" to "=== functions", with the frame rate the front end has been set to and whether the pane runs code
void dumpSession(std::ostream& out, Session& session, float targetFrameRate, bool sessionActive);

#endif
