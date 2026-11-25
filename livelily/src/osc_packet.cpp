#include "osc_packet.h"
#include <cstdint>
#include <cstring>

namespace {

// OSC is big-endian, and its strings and blobs are padded with zeros to a multiple of four bytes

void putUint32(std::string& out, uint32_t value)
{
	for (int shift = 24; shift >= 0; shift -= 8) out += (char)((value >> shift) & 0xFF);
}

void putUint64(std::string& out, uint64_t value)
{
	putUint32(out, (uint32_t)(value >> 32));
	putUint32(out, (uint32_t)(value & 0xFFFFFFFF));
}

void putString(std::string& out, const std::string& s)
{
	// a string ends at its first zero byte
	out.append(s.c_str());
	size_t padding = 4 - (std::strlen(s.c_str()) % 4);
	out.append(padding, '\0');
}

// reads a packet from start to end, and gives up at the first byte that doesn't fit
class Reader
{
	public:
		Reader(const char* data, size_t size) : pos(data), end(data + size) {}
		bool atEnd() const { return pos == end; }
		size_t left() const { return (size_t)(end - pos); }
		bool uint32(uint32_t& value)
		{
			if (left() < 4) return false;
			const unsigned char* b = (const unsigned char*)pos;
			value = ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | (uint32_t)b[3];
			pos += 4;
			return true;
		}
		bool uint64(uint64_t& value)
		{
			uint32_t high, low;
			if (!uint32(high) || !uint32(low)) return false;
			value = ((uint64_t)high << 32) | low;
			return true;
		}
		bool string(std::string& s)
		{
			const char* zero = (const char*)std::memchr(pos, '\0', left());
			if (zero == nullptr) return false;
			size_t length = (size_t)(zero - pos);
			size_t padded = (length / 4 + 1) * 4;
			if (left() < padded) return false;
			s.assign(pos, length);
			pos += padded;
			return true;
		}
		bool skip(size_t n)
		{
			if (left() < n) return false;
			pos += n;
			return true;
		}
	private:
		const char* pos;
		const char* end;
};

bool decodeMessage(const char* data, size_t size, OscMessage& m)
{
	Reader r(data, size);
	std::string address, tags;
	if (!r.string(address) || address.empty() || address[0] != '/') return false;
	m.setAddress(address);
	// a message with no type tags (from very old programs) has no arguments
	if (r.atEnd()) return true;
	if (!r.string(tags) || tags.empty() || tags[0] != ',') return false;
	for (size_t i = 1; i < tags.size(); i++) {
		uint32_t u32;
		uint64_t u64;
		std::string s;
		switch (tags[i]) {
			case 'i':
				if (!r.uint32(u32)) return false;
				m.addIntArg((int32_t)u32);
				break;
			case 'h':
				if (!r.uint64(u64)) return false;
				m.addInt64Arg((int64_t)u64);
				break;
			case 'f': {
				if (!r.uint32(u32)) return false;
				float f;
				std::memcpy(&f, &u32, 4);
				m.addFloatArg(f);
				break;
			}
			case 'd': {
				if (!r.uint64(u64)) return false;
				double d;
				std::memcpy(&d, &u64, 8);
				m.addDoubleArg(d);
				break;
			}
			case 's':
				if (!r.string(s)) return false;
				m.addStringArg(s);
				break;
			case 'S':
				if (!r.string(s)) return false;
				m.addSymbolArg(s);
				break;
			case 'T': m.addBoolArg(true); break;
			case 'F': m.addBoolArg(false); break;
			case 'b':
				if (!r.uint32(u32) || !r.skip(((size_t)u32 + 3) / 4 * 4)) return false;
				m.addOtherArg();
				break;
			case 'c': case 'r': case 'm':
				if (!r.uint32(u32)) return false;
				m.addOtherArg();
				break;
			case 't':
				if (!r.uint64(u64)) return false;
				m.addOtherArg();
				break;
			case 'N': case 'I':
				m.addOtherArg();
				break;
			// the brackets of arrays have no data, and their elements are read as arguments of their own
			case '[': case ']':
				break;
			default:
				// a type whose size isn't known: what follows can't be read
				return false;
		}
	}
	return true;
}

bool decodeElement(const char* data, size_t size, std::vector<OscMessage>& messages, int depth)
{
	// bundles inside bundles, up to a depth that no sender needs, so that a crafted packet can't exhaust the stack
	if (depth > 8) return false;
	if (size >= 8 && std::memcmp(data, "#bundle", 8) == 0) {
		Reader r(data + 8, size - 8);
		uint64_t timeTag;
		if (!r.uint64(timeTag)) return false;
		size_t offset = 16;
		while (offset < size) {
			Reader sizeReader(data + offset, size - offset);
			uint32_t elementSize;
			if (!sizeReader.uint32(elementSize) || elementSize % 4 != 0 || elementSize > size - offset - 4) return false;
			if (!decodeElement(data + offset + 4, elementSize, messages, depth + 1)) return false;
			offset += 4 + elementSize;
		}
		return true;
	}
	OscMessage m;
	if (!decodeMessage(data, size, m)) return false;
	messages.push_back(m);
	return true;
}

}

namespace OscPacket {

std::string encode(const OscMessage& m)
{
	std::string tags = ",";
	std::string args;
	for (size_t i = 0; i < m.getNumArgs(); i++) {
		switch (m.getArgType(i)) {
			case OscArgType::Int32:
				tags += 'i';
				putUint32(args, (uint32_t)m.getArgAsInt32(i));
				break;
			case OscArgType::Int64:
				tags += 'h';
				putUint64(args, (uint64_t)m.getArgAsInt64(i));
				break;
			case OscArgType::Float: {
				tags += 'f';
				float f = m.getArgAsFloat(i);
				uint32_t u;
				std::memcpy(&u, &f, 4);
				putUint32(args, u);
				break;
			}
			case OscArgType::Double: {
				tags += 'd';
				double d = m.getArgAsDouble(i);
				uint64_t u;
				std::memcpy(&u, &d, 8);
				putUint64(args, u);
				break;
			}
			case OscArgType::String:
				tags += 's';
				putString(args, m.getArgAsString(i));
				break;
			case OscArgType::Symbol:
				tags += 'S';
				putString(args, m.getArgAsString(i));
				break;
			case OscArgType::True: tags += 'T'; break;
			case OscArgType::False: tags += 'F'; break;
			default: break;
		}
	}
	std::string out;
	putString(out, m.getAddress());
	putString(out, tags);
	return out + args;
}

bool decode(const char* data, size_t size, std::vector<OscMessage>& messages)
{
	// every element of OSC is a multiple of four bytes
	if (size == 0 || size % 4 != 0) return false;
	std::vector<OscMessage> decoded;
	if (!decodeElement(data, size, decoded, 0)) return false;
	messages.insert(messages.end(), decoded.begin(), decoded.end());
	return true;
}

}
