// checks LiveLily's OSC (see src/osc_packet.cpp and src/osc_udp.cpp): packets written and read, and messages sent through a socket
// run by CTest (the test osc), with no test files, so it runs everywhere
#include "osc.h"
#include "osc_packet.h"
#include <chrono>
#include <cmath>
#include <iostream>
#include <thread>

namespace {

int failures = 0;

void check(bool ok, const std::string& what)
{
	if (!ok) {
		std::cerr << "FAILED: " << what << std::endl;
		failures++;
	}
}

std::string bytes(std::initializer_list<int> values)
{
	std::string s;
	for (int v : values) s += (char)v;
	return s;
}

// a message with every type LiveLily writes
OscMessage everyType()
{
	OscMessage m;
	m.setAddress("/livelily/test");
	m.addIntArg(-42);
	m.addInt64Arg(1234567890123LL);
	m.addFloatArg(0.5f);
	m.addDoubleArg(3.25);
	m.addStringArg("abc");
	m.addSymbolArg("symbol");
	m.addBoolArg(true);
	m.addBoolArg(false);
	m.addStringArg("");
	return m;
}

void checkEveryType(const OscMessage& m, const std::string& where)
{
	check(m.getAddress() == "/livelily/test", where + ": address");
	check(m.getNumArgs() == 9, where + ": number of arguments");
	if (m.getNumArgs() != 9) return;
	check(m.getArgType(0) == OscArgType::Int32 && m.getArgAsInt32(0) == -42, where + ": int32");
	check(m.getArgType(1) == OscArgType::Int64 && m.getArgAsInt64(1) == 1234567890123LL, where + ": int64");
	check(m.getArgType(2) == OscArgType::Float && m.getArgAsFloat(2) == 0.5f, where + ": float");
	check(m.getArgType(3) == OscArgType::Double && m.getArgAsDouble(3) == 3.25, where + ": double");
	check(m.getArgType(4) == OscArgType::String && m.getArgAsString(4) == "abc", where + ": string");
	check(m.getArgType(5) == OscArgType::Symbol && m.getArgAsString(5) == "symbol", where + ": symbol");
	check(m.getArgType(6) == OscArgType::True && m.getArgAsBool(6), where + ": true");
	check(m.getArgType(7) == OscArgType::False && !m.getArgAsBool(7), where + ": false");
	check(m.getArgType(8) == OscArgType::String && m.getArgAsString(8).empty(), where + ": empty string");
}

void packets()
{
	// the example of the OSC 1.0 specification
	OscMessage spec;
	spec.setAddress("/foo");
	spec.addIntArg(1000);
	spec.addIntArg(-1);
	spec.addStringArg("hello");
	spec.addFloatArg(1.234f);
	spec.addFloatArg(5.678f);
	std::string expected = bytes({0x2f, 0x66, 0x6f, 0x6f, 0, 0, 0, 0, 0x2c, 0x69, 0x69, 0x73, 0x66, 0x66, 0, 0, 0, 0, 0x03, 0xe8,
		0xff, 0xff, 0xff, 0xff, 0x68, 0x65, 0x6c, 0x6c, 0x6f, 0, 0, 0, 0x3f, 0x9d, 0xf3, 0xb6, 0x40, 0xb5, 0xb2, 0x2d});
	check(OscPacket::encode(spec) == expected, "the example of the specification");

	std::string packet = OscPacket::encode(everyType());
	check(packet.size() % 4 == 0, "a packet is a multiple of four bytes");
	std::vector<OscMessage> decoded;
	check(OscPacket::decode(packet.data(), packet.size(), decoded) && decoded.size() == 1, "a message is read back");
	if (decoded.size() == 1) checkEveryType(decoded[0], "read back");

	// a bundle with a message and a bundle with a message in it
	OscMessage a, b;
	a.setAddress("/a");
	a.addIntArg(1);
	b.setAddress("/b");
	b.addStringArg("x");
	std::string pa = OscPacket::encode(a), pb = OscPacket::encode(b);
	auto sizeOf = [](const std::string& s) { return bytes({0, 0, 0, (int)s.size()}); };
	std::string timeTag = bytes({0, 0, 0, 0, 0, 0, 0, 1});
	std::string inner = std::string("#bundle\0", 8) + timeTag + sizeOf(pb) + pb;
	std::string bundle = std::string("#bundle\0", 8) + timeTag + sizeOf(pa) + pa + sizeOf(inner) + inner;
	decoded.clear();
	check(OscPacket::decode(bundle.data(), bundle.size(), decoded) && decoded.size() == 2, "the messages of a bundle are read");
	if (decoded.size() == 2) {
		check(decoded[0].getAddress() == "/a" && decoded[0].getArgAsInt32(0) == 1, "the first message of a bundle");
		check(decoded[1].getAddress() == "/b" && decoded[1].getArgAsString(0) == "x", "the message of the bundle in a bundle");
	}

	// arguments LiveLily doesn't read keep the places of the others: a blob of 5 bytes, nil, and a time tag
	std::string others = bytes({'/', 'o', 0, 0, ',', 'b', 'N', 't', 'i', 0, 0, 0, 0, 0, 0, 5, 1, 2, 3, 4, 5, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 7});
	decoded.clear();
	check(OscPacket::decode(others.data(), others.size(), decoded) && decoded.size() == 1, "a message with a blob, nil and a time tag");
	if (decoded.size() == 1) {
		check(decoded[0].getNumArgs() == 4 && decoded[0].getArgType(0) == OscArgType::Other && decoded[0].getArgType(2) == OscArgType::Other,
			"arguments LiveLily doesn't read are Other");
		check(decoded[0].getArgType(3) == OscArgType::Int32 && decoded[0].getArgAsInt32(3) == 7, "the argument after them");
	}

	// malformed packets are refused, and add nothing
	decoded.clear();
	check(!OscPacket::decode(packet.data(), packet.size() - 4, decoded) && decoded.empty(), "a cut packet is refused");
	std::string noSlash = bytes({'a', 0, 0, 0, ',', 0, 0, 0});
	check(!OscPacket::decode(noSlash.data(), noSlash.size(), decoded) && decoded.empty(), "an address without a slash is refused");
	std::string unknownType = bytes({'/', 0, 0, 0, ',', 'X', 0, 0, 0, 0, 0, 0});
	check(!OscPacket::decode(unknownType.data(), unknownType.size(), decoded) && decoded.empty(), "an unknown type is refused");
	std::string badBundle = std::string("#bundle\0", 8) + timeTag + bytes({0, 0, 1, 0}) + pa;
	check(!OscPacket::decode(badBundle.data(), badBundle.size(), decoded) && decoded.empty(), "a bundle element too long is refused");
}

void socketRoundTrip()
{
	std::shared_ptr<OscReceiver> receiver = makeOscReceiver();
	int port = 0;
	for (int p = 47120; p < 47140 && port == 0; p++) if (receiver->setup(p)) port = p;
	check(port != 0, "a receiver gets a port");
	if (port == 0) return;
	std::shared_ptr<OscSender> sender = makeOscSender();
	check(sender->setup("localhost", port), "a sender is set up to localhost");
	check(!sender->setup("", port), "a sender to no host isn't set up");
	check(sender->setup("127.0.0.1", port), "a sender is set up to 127.0.0.1");
	check(!receiver->hasWaitingMessages(), "nothing has arrived before sending");
	sender->send(everyType());
	OscMessage second;
	second.setAddress("/second");
	sender->send(second);
	auto start = std::chrono::steady_clock::now();
	std::vector<OscMessage> received;
	while (received.size() < 2 && std::chrono::steady_clock::now() - start < std::chrono::seconds(2)) {
		OscMessage m;
		if (receiver->getNextMessage(m)) received.push_back(m);
		else std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}
	check(received.size() == 2, "both messages arrive through the socket");
	if (received.size() == 2) {
		checkEveryType(received[0], "through the socket");
		check(received[1].getAddress() == "/second" && received[1].getNumArgs() == 0, "the second message, in order");
	}
}

}

int main()
{
	packets();
	socketRoundTrip();
	if (failures == 0) std::cout << "osc: all checks passed" << std::endl;
	return failures == 0 ? 0 : 1;
}
