#include "of_midi.h"
#include <algorithm>

std::vector<std::string> OfMidiOutput::portNames()
{
	return lister.getOutPortList();
}

bool OfMidiOutput::open(int port)
{
	if (port < 0) return false;
	if (isOpen(port)) return true;
	auto midiOut = std::make_unique<ofxMidiOut>();
	// the port is kept as before, even if ofxMidi couldn't open it, which then ignores what is sent to it
	bool opened = midiOut->openPort((unsigned int)port);
	outs[port] = std::move(midiOut);
	return opened;
}

bool OfMidiOutput::isOpen(int port)
{
	return outs.find(port) != outs.end();
}

void OfMidiOutput::closeAll()
{
	for (auto& out : outs) out.second->closePort();
	outs.clear();
}

ofxMidiOut* OfMidiOutput::out(int port)
{
	auto it = outs.find(port);
	return (it != outs.end() ? it->second.get() : nullptr);
}

void OfMidiOutput::noteOn(int port, int channel, int note, int velocity)
{
	if (ofxMidiOut* o = out(port)) o->sendNoteOn(channel, note, velocity);
}

void OfMidiOutput::noteOff(int port, int channel, int note, int velocity)
{
	if (ofxMidiOut* o = out(port)) o->sendNoteOff(channel, note, velocity);
}

void OfMidiOutput::pitchBend(int port, int channel, int value)
{
	// LiveLily gives -8192 to 8191 (0 for no bend, see midi.h), and ofxMidi expects 0 to 16383 (8192 for no bend)
	if (ofxMidiOut* o = out(port)) o->sendPitchBend(channel, std::clamp(value + 8192, 0, 16383));
}

void OfMidiOutput::programChange(int port, int channel, int program)
{
	if (ofxMidiOut* o = out(port)) o->sendProgramChange(channel, program);
}

void OfMidiOutput::controlChange(int port, int channel, int control, int value)
{
	if (ofxMidiOut* o = out(port)) o->sendControlChange(channel, control, value);
}

void OfMidiOutput::sendByte(int port, unsigned char byte)
{
	if (ofxMidiOut* o = out(port)) o->sendMidiByte(byte);
}
