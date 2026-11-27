#ifndef OSC_PACKET_H
#define OSC_PACKET_H

#include "osc.h"
#include <cstddef>
#include <string>
#include <vector>

// the packets of OSC 1.0 (see osc_udp.cpp, which sends and receives them)
namespace OscPacket {

// a message as the bytes of a packet; arguments of the type Other are left out, as ofxOsc did
std::string encode(const OscMessage& m);

// the messages of a packet, a message or a bundle (with the messages of the bundles in it, in order), added to messages
// arguments LiveLily doesn't read (blobs, time tags, characters, colours, MIDI messages, nil, impulses) are Other arguments
// returns false if the packet is malformed, and then nothing is added
bool decode(const char* data, size_t size, std::vector<OscMessage>& messages);

}

#endif
