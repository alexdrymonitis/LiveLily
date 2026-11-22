#include <fstream>
#include <sstream>
#include <map>
#include <vector>
#include <string>
#include <cmath>
#include <cctype>
#include <algorithm>
#include <iterator>
#include <utility>
#include <cstdint>
#include <zlib.h> // to decompress compressed MusicXML files (.mxl)
#include "musicxml.h"
#include "utils.h"

/********************* MusicXMLLoader class *********************/
//--------------------------------------------------------------
MusicXMLLoader::MusicXMLLoader(const std::string& tab)
{
	tabStr = tab;
	cursorLineIndex = 0;
}

//--------------------------------------------------------------
std::vector<std::string> MusicXMLLoader::load(const std::string& filePath)
{
	lines.clear();
	cursorLineIndex = 0;
	error.clear();
	std::ifstream inFile(filePath.c_str(), std::ios::binary);
	if (!inFile.is_open()) {
		error = "could not load file";
		return lines;
	}
	std::stringstream fileContents;
	fileContents << inFile.rdbuf();
	inFile.close();
	std::string xml = fileContents.str();
	// a compressed MusicXML file is recognized by its contents, and not by its extension
	if (isZipArchive(xml)) {
		xml = musicXMLFromMxl(xml);
		if (xml.empty()) return lines;
	}
	// the converter reads the parts one after the other (partwise MusicXML)
	if (xml.find("<score-partwise") == std::string::npos) {
		if (xml.find("<score-timewise") != std::string::npos) error = "timewise MusicXML is not supported";
		else error = "not a MusicXML file";
		return lines;
	}
	convert(xml);
	return lines;
}

//--------------------------------------------------------------
std::string MusicXMLLoader::getError()
{
	return error;
}

//--------------------------------------------------------------
bool MusicXMLLoader::isZipArchive(const std::string& data)
{
	// every zip archive starts with the signature of a local file header, "PK\3\4"
	return data.size() >= 4 && data.compare(0, 4, std::string("PK\x03\x04", 4)) == 0;
}

//--------------------------------------------------------------
// reads little-endian integers from the zip data, returning 0 past the end of the data
static uint32_t zipRead16(const std::string& data, size_t pos)
{
	if (pos + 2 > data.size()) return 0;
	return (uint32_t)(unsigned char)data[pos] | ((uint32_t)(unsigned char)data[pos+1] << 8);
}

static uint32_t zipRead32(const std::string& data, size_t pos)
{
	if (pos + 4 > data.size()) return 0;
	return zipRead16(data, pos) | (zipRead16(data, pos + 2) << 16);
}

// the location of an entry in the zip data, from the central directory
struct ZipEntry
{
	std::string name;
	uint32_t method;
	uint32_t compressedSize;
	uint32_t uncompressedSize;
	uint32_t localHeaderOffset;
};

// reads the central directory at the end of the archive, which lists all its entries with their sizes
// (the sizes in the local headers can be 0, when they are written after the data instead)
static std::vector<ZipEntry> zipCentralDirectory(const std::string& data)
{
	std::vector<ZipEntry> entries;
	// the end of central directory record is 22 bytes, plus a comment of up to 65535 bytes, at the end of the archive
	if (data.size() < 22) return entries;
	size_t searchStart = (data.size() > 22 + 65535 ? data.size() - 22 - 65535 : 0);
	size_t eocd = std::string::npos;
	for (size_t i = data.size() - 22 + 1; i-- > searchStart; ) {
		if (zipRead32(data, i) == 0x06054b50) {
			eocd = i;
			break;
		}
	}
	if (eocd == std::string::npos) return entries;
	uint32_t numEntries = zipRead16(data, eocd + 10);
	size_t pos = zipRead32(data, eocd + 16);
	for (uint32_t i = 0; i < numEntries; i++) {
		if (zipRead32(data, pos) != 0x02014b50) break;
		ZipEntry entry;
		entry.method = zipRead16(data, pos + 10);
		entry.compressedSize = zipRead32(data, pos + 20);
		entry.uncompressedSize = zipRead32(data, pos + 24);
		uint32_t nameLength = zipRead16(data, pos + 28);
		uint32_t extraLength = zipRead16(data, pos + 30);
		uint32_t commentLength = zipRead16(data, pos + 32);
		entry.localHeaderOffset = zipRead32(data, pos + 42);
		if (pos + 46 + nameLength > data.size()) break;
		entry.name = data.substr(pos + 46, nameLength);
		entries.push_back(entry);
		pos += 46 + nameLength + extraLength + commentLength;
	}
	return entries;
}

//--------------------------------------------------------------
std::vector<std::string> MusicXMLLoader::listZipEntries(const std::string& data)
{
	std::vector<std::string> names;
	for (const ZipEntry& entry : zipCentralDirectory(data)) names.push_back(entry.name);
	return names;
}

//--------------------------------------------------------------
std::string MusicXMLLoader::extractFromZip(const std::string& data, const std::string& entryName)
{
	// returns the uncompressed contents of an entry of the archive
	// entries are either stored as they are (method 0) or compressed with deflate (method 8), as MusicXML requires
	for (const ZipEntry& entry : zipCentralDirectory(data)) {
		if (entry.name != entryName) continue;
		size_t header = entry.localHeaderOffset;
		if (zipRead32(data, header) != 0x04034b50) {
			error = "damaged compressed MusicXML file";
			return "";
		}
		// the data follows the local header, which has its own name and extra field lengths
		size_t start = header + 30 + zipRead16(data, header + 26) + zipRead16(data, header + 28);
		if (start + entry.compressedSize > data.size()) {
			error = "damaged compressed MusicXML file";
			return "";
		}
		if (entry.method == 0) {
			return data.substr(start, entry.compressedSize);
		}
		if (entry.method != 8) {
			error = "unsupported compression in MusicXML file";
			return "";
		}
		std::string out;
		out.resize(entry.uncompressedSize);
		z_stream stream = {};
		stream.next_in = (Bytef*)(data.data() + start);
		stream.avail_in = entry.compressedSize;
		stream.next_out = (Bytef*)&out[0];
		stream.avail_out = entry.uncompressedSize;
		// a negative window size means raw deflate data, without a zlib header, as stored in zip archives
		if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) {
			error = "could not decompress MusicXML file";
			return "";
		}
		int result = inflate(&stream, Z_FINISH);
		inflateEnd(&stream);
		if (result != Z_STREAM_END) {
			error = "could not decompress MusicXML file";
			return "";
		}
		out.resize(stream.total_out);
		return out;
	}
	error = "file " + entryName + " not found in compressed MusicXML file";
	return "";
}

//--------------------------------------------------------------
std::string MusicXMLLoader::musicXMLFromMxl(const std::string& data)
{
	// the file META-INF/container.xml names the score file of the archive, in the first <rootfile> element
	// e.g. <rootfile full-path="score.xml" media-type="application/vnd.recordare.musicxml+xml"/>
	// other rootfile elements can point to other formats (e.g. a PDF of the score), so the MusicXML one is chosen
	// the list of the entries is at the end of the archive, so an archive that has been cut short has no entries
	if (listZipEntries(data).empty()) {
		error = "damaged compressed MusicXML file";
		return "";
	}
	std::string scorePath;
	std::string container = extractFromZip(data, "META-INF/container.xml");
	error.clear();
	size_t pos = 0;
	while ((pos = container.find("<rootfile", pos)) != std::string::npos) {
		size_t end = container.find('>', pos);
		if (end == std::string::npos) break;
		std::string tag = container.substr(pos, end - pos + 1);
		std::string path = xmlAttribute(tag, "full-path");
		std::string mediaType = xmlAttribute(tag, "media-type");
		if (!path.empty() && (mediaType.empty() || mediaType.find("musicxml") != std::string::npos)) {
			scorePath = path;
			break;
		}
		pos = end;
	}
	// without a container file, use the first MusicXML file of the archive that is not in META-INF
	if (scorePath.empty()) {
		for (const std::string& name : listZipEntries(data)) {
			if (!startsWith(name, "META-INF/") && (endsWith(name, ".xml") || endsWith(name, ".musicxml"))) {
				scorePath = name;
				break;
			}
		}
	}
	if (scorePath.empty()) {
		error = "no score found in compressed MusicXML file";
		return "";
	}
	std::string xml = extractFromZip(data, scorePath);
	if (xml.empty() && error.empty()) error = "the score in the compressed MusicXML file is empty";
	return xml;
}

//--------------------------------------------------------------
void MusicXMLLoader::createNewLine(const std::string& str, int increment)
{
	// writes a line at the index of the cursor plus the increment, like Editor::createNewLine()
	// so the converting code can be the same as when it was writing straight to the editor
	int index = cursorLineIndex + increment;
	if (index < 0) return;
	if ((int)lines.size() <= index) lines.resize(index + 1);
	lines[index] = str;
}

//--------------------------------------------------------------
int MusicXMLLoader::xmlInt(const std::string& str, int fallback)
{
	// parses the integer at the start of a string (e.g. the "3" of "3</beats>")
	// returns fallback if the string doesn't start with one, so malformed files don't throw exceptions
	size_t i = 0;
	bool negative = false;
	if (i < str.size() && (str[i] == '-' || str[i] == '+')) {
		negative = (str[i] == '-');
		i++;
	}
	if (i >= str.size() || !isdigit(str[i])) return fallback;
	long value = 0;
	// limit the number of digits so the value can't overflow
	for (int numDigits = 0; i < str.size() && isdigit(str[i]) && numDigits < 9; i++, numDigits++) {
		value = value * 10 + (str[i] - '0');
	}
	return (int)(negative ? -value : value);
}

//--------------------------------------------------------------
std::string MusicXMLLoader::xmlAttribute(const std::string& line, const std::string& name)
{
	// returns the value of an attribute of an XML element, e.g. "P1" for name "id" in <score-part id="P1">
	// or an empty string if the attribute is not in the line
	// the line must include its white spaces, as attribute names are preceded by white space
	size_t pos = 0;
	while ((pos = line.find(name, pos)) != std::string::npos) {
		size_t afterName = pos + name.size();
		bool wholeName = (pos > 0 && isspace(line[pos-1]));
		pos = afterName;
		if (!wholeName) continue;
		// skip white space around the equals sign
		size_t eqNdx = line.find_first_not_of(" \t", afterName);
		if (eqNdx == std::string::npos || line[eqNdx] != '=') continue;
		size_t quoteNdx = line.find_first_not_of(" \t", eqNdx + 1);
		if (quoteNdx == std::string::npos || (line[quoteNdx] != '"' && line[quoteNdx] != '\'')) continue;
		size_t closingQuoteNdx = line.find(line[quoteNdx], quoteNdx + 1);
		if (closingQuoteNdx == std::string::npos) return "";
		return line.substr(quoteNdx + 1, closingQuoteNdx - quoteNdx - 1);
	}
	return "";
}

//--------------------------------------------------------------
bool MusicXMLLoader::isXmlElement(const std::string& line, const std::string& name)
{
	// checks if a line starts with an element with the given name, with or without attributes
	// the name must be followed by white space, '>', or '/'
	// so that e.g. "score-part" doesn't match <score-partwise>, and "measure" doesn't match <measure-style>
	// the line must include its white spaces
	size_t start = line.find_first_not_of(" \t");
	if (start == std::string::npos || line.compare(start, name.size() + 1, "<" + name) != 0) return false;
	size_t afterName = start + name.size() + 1;
	if (afterName >= line.size()) return true;
	char c = line[afterName];
	return (isspace(c) || c == '>' || c == '/');
}

//--------------------------------------------------------------
int MusicXMLLoader::xmlMeasureNumber(const std::string& line, int prevNumber)
{
	// the number of a measure, which is usually a number, but can also be e.g. "X1" for measures that are not counted
	// in which case we use the number after the previous measure
	return xmlInt(xmlAttribute(line, "number"), prevNumber + 1);
}

//--------------------------------------------------------------
std::string MusicXMLLoader::xmlElementText(const std::string& line)
{
	// returns the text of an element written on one line, e.g. "130" for <per-minute font-size="12">130</per-minute>
	// without the attributes, and with white space at the edges stripped
	// an empty element (e.g. <beat-unit-dot/>) or an element whose text is not on the same line returns an empty string
	size_t start = line.find('<');
	if (start == std::string::npos) return "";
	size_t tagEnd = line.find('>', start);
	if (tagEnd == std::string::npos || line[tagEnd-1] == '/') return "";
	size_t closing = line.find("</", tagEnd);
	if (closing == std::string::npos) return "";
	std::string text = line.substr(tagEnd + 1, closing - tagEnd - 1);
	size_t first = text.find_first_not_of(" \t");
	if (first == std::string::npos) return "";
	size_t last = text.find_last_not_of(" \t");
	return text.substr(first, last - first + 1);
}

//--------------------------------------------------------------
std::string MusicXMLLoader::xmlText(const std::string& str)
{
	// converts the text of an XML element to text that can be written in a LiveLily string
	// replaces the XML entities (e.g. &amp;) with their characters, and double quotes with single ones
	// as a double quote would end the LiveLily string
	// and strips white space at the edges
	static const std::vector<std::pair<std::string, std::string>> entities = {
		{"&quot;", "'"}, {"&apos;", "'"}, {"&lt;", "<"}, {"&gt;", ">"}, {"&amp;", "&"}};
	std::string text;
	for (size_t i = 0; i < str.size(); i++) {
		bool replaced = false;
		if (str[i] == '&') {
			for (const auto& entity : entities) {
				if (str.compare(i, entity.first.size(), entity.first) == 0) {
					text += entity.second;
					i += entity.first.size() - 1;
					replaced = true;
					break;
				}
			}
		}
		if (!replaced) text += (str[i] == '"' ? '\'' : str[i]);
	}
	size_t first = text.find_first_not_of(" \t");
	if (first == std::string::npos) return "";
	size_t last = text.find_last_not_of(" \t");
	return text.substr(first, last - first + 1);
}

//--------------------------------------------------------------
std::string MusicXMLLoader::orderNoteMarks(const std::string& marks)
{
	// orders the marks of a note (hairpins, dynamics, and text) the way LiveLily reads them
	// a dynamic after the start of a hairpin ends the hairpin, so on the same note
	// the stop of a hairpin (\!) comes first, then dynamics, then text, and the start of a hairpin (\< or \>) comes last
	// e.g. a diminuendo and a mezzo forte on the same note are written as \mf\>, not \>\mf
	std::string stops, dynamics, texts, starts;
	// a hairpin that starts and stops on the same note (the start comes before the stop) lasts no time, so it's dropped
	// (a stop before a start on the same note ends one hairpin and starts another, which is fine)
	int startsWithoutStop = 0;
	size_t i = 0;
	while (i < marks.size()) {
		// text, e.g. ^"accel." or _"dolce"
		if ((marks[i] == '^' || marks[i] == '_') && i + 1 < marks.size() && marks[i+1] == '"') {
			size_t closingQuote = marks.find('"', i + 2);
			size_t end = (closingQuote == std::string::npos ? marks.size() : closingQuote + 1);
			texts += marks.substr(i, end - i);
			i = end;
		}
		else if (marks[i] == '\\' && i + 1 < marks.size() && (marks[i+1] == '<' || marks[i+1] == '>')) {
			starts += marks.substr(i, 2);
			startsWithoutStop++;
			i += 2;
		}
		else if (marks[i] == '\\' && i + 1 < marks.size() && marks[i+1] == '!') {
			if (startsWithoutStop > 0) {
				// drop the last start, and this stop
				starts.erase(starts.size() - 2);
				startsWithoutStop--;
			}
			else {
				stops += marks.substr(i, 2);
			}
			i += 2;
		}
		// a dynamic, e.g. \mf
		else if (marks[i] == '\\') {
			size_t end = i + 1;
			while (end < marks.size() && isalpha(marks[end])) end++;
			dynamics += marks.substr(i, end - i);
			i = end;
		}
		else {
			texts += marks[i];
			i++;
		}
	}
	return stops + dynamics + texts + starts;
}

//--------------------------------------------------------------
size_t MusicXMLLoader::findChordEnd(std::string str)
{
	// returns the position of the closing angle bracket of the last chord
	// or the end of the string if there is none, so the result can always be used to insert in the string
	size_t ndx = str.find_last_of(">");
	if (ndx == std::string::npos) return str.size();
	if (ndx + 1 < str.size()) {
		if (str[ndx+1] == '}') {
			ndx = str.substr(0, ndx).find_last_of(" ");
			if (ndx == std::string::npos) return str.size();
		}
	}
	if (ndx > 0) {
		// if the previous character is a hyphen
		// it means that this ">" symbol is an articulation symbol
		// or if the previous character is a backslash
		// it means that this ">" symbol is a diminuendo
		// not a chord end symbol
		if (str[ndx-1] == '-' || str[ndx-1] == '\\') {
			ndx = str.substr(0, ndx).find_last_of(">");
			if (ndx == std::string::npos) return str.size();
		}
	}
	return ndx;
}

//--------------------------------------------------------------
size_t MusicXMLLoader::findChordStart(std::string str)
{
	size_t ndx = str.find_last_of("<");
	// npos means there is no chord start, which callers check for
	if (ndx != std::string::npos && ndx > 0) {
		// if the previous character is a hyphen
		// it means that this ">" symbol is an articulation symbol
		// or if the previous character is a backslash
		// it means that this ">" symbol is a diminuendo
		// not a chord end symbol
		if (str[ndx-1] == '-' || str[ndx-1] == '\\') {
			ndx = str.substr(0, ndx).find_last_of("<");
		}
	}
	return ndx;
}

/************************ loading files ***********************/

//--------------------------------------------------------------
std::string MusicXMLLoader::oneElementPerLine(const std::string& xml)
{
	// rewrites XML so that every tag starts a new line, unless it closes an element that has text
	// e.g. <metronome><beat-unit>quarter</beat-unit><per-minute>120</per-minute></metronome> becomes
	// <metronome>
	// <beat-unit>quarter</beat-unit>
	// <per-minute>120</per-minute>
	// </metronome>
	// white space between tags (indentation and line breaks) is dropped, as it means nothing in MusicXML
	// line breaks inside the text of an element become spaces, so that an element with its text is on one line
	// comments, CDATA sections, and the document type declaration are copied as they are, on their own line
	std::string out;
	out.reserve(xml.size() + xml.size() / 4);
	// whether the last thing written is text inside an element, in which case the following closing tag stays on its line
	bool afterText = false;
	size_t i = 0;
	while (i < xml.size()) {
		if (xml[i] != '<') {
			// text between tags
			size_t next = xml.find('<', i);
			if (next == std::string::npos) next = xml.size();
			std::string text = xml.substr(i, next - i);
			if (text.find_first_not_of(" \t\r\n") != std::string::npos) {
				for (char& c : text) {
					if (c == '\r' || c == '\n') c = ' ';
				}
				out += text;
				afterText = true;
			}
			i = next;
			continue;
		}
		// find the end of the tag
		size_t end;
		if (xml.compare(i, 4, "<!--") == 0) {
			end = xml.find("-->", i + 4);
			end = (end == std::string::npos ? xml.size() : end + 3);
		}
		else if (xml.compare(i, 9, "<![CDATA[") == 0) {
			end = xml.find("]]>", i + 9);
			end = (end == std::string::npos ? xml.size() : end + 3);
		}
		else {
			// a '>' inside a quoted attribute value, or inside the brackets of a document type declaration, doesn't end the tag
			char quote = 0;
			int brackets = 0;
			end = i + 1;
			while (end < xml.size()) {
				char c = xml[end];
				if (quote) {
					if (c == quote) quote = 0;
				}
				else if (c == '"' || c == '\'') quote = c;
				else if (c == '[') brackets++;
				else if (c == ']' && brackets > 0) brackets--;
				else if (c == '>' && brackets == 0) break;
				end++;
			}
			end = (end < xml.size() ? end + 1 : xml.size());
		}
		std::string tag = xml.substr(i, end - i);
		bool closingTag = (tag.size() > 1 && tag[1] == '/');
		// a closing tag right after the text of its element stays on the same line, every other tag starts a new line
		if (!(closingTag && afterText) && !out.empty() && out.back() != '\n') out += '\n';
		out += tag;
		afterText = false;
		i = end;
	}
	out += '\n';
	return out;
}

/////////////////// loading MusicXML files /////////////////////

//--------------------------------------------------------------
void MusicXMLLoader::convert(const std::string& xml)
{
	// the converter reads the file line by line and expects every element on its own line
	// which is how most notation software writes MusicXML, but not all files are written like this
	// so the file is first rewritten with one element per line, and read from memory
	std::istringstream file(oneElementPerLine(xml));
	std::string line;
	createNewLine("\\score.show", 0);
	cursorLineIndex++;
	createNewLine("\\score.animate", 0);
	cursorLineIndex++;
	createNewLine("", 0);
	cursorLineIndex++;
	int partID = 0, prevPartID = 0;
	bool mustMoveKeys = false;
	int partIDOffset = -1;
	int numStaves = 1, loopIter, i;
	size_t ndx1, ndx2;
	unsigned int lineNum = 1;
	std::map<int, std::pair<int, std::pair<std::string, std::string>>> insts;
	// the number of each part, by its id
	// ids are usually "P" followed by a number, in which case we use that number
	// otherwise we number the parts in the order they appear
	std::map<std::string, int> partNumbers;
	// groups of parts from <part-group> elements in the part list (e.g. a brace or bracket over several parts)
	// the open groups are stored by their number, as groups can overlap or be nested
	// each group stores the IDs of its parts, and whether it is displayed (a group symbol "none" is not)
	std::map<std::string, std::pair<std::vector<std::string>, bool>> openPartGroups;
	std::string lastOpenedPartGroup;
	std::vector<std::vector<std::string>> partGroups;
	while (getline(file, line)) {
		// files with Windows line endings (CRLF) leave a carriage return at the end of each line
		if (!line.empty() && line.back() == '\r') line.pop_back();
		std::string lineWithoutTabs = replaceCharInStr(line, "\t", "");
		std::string lineWithoutSpaces = replaceCharInStr(lineWithoutTabs, " ", "");
		if (isXmlElement(line, "part-group")) {
			std::string groupNumber = xmlAttribute(line, "number");
			std::string groupType = xmlAttribute(line, "type");
			if (groupType == "start") {
				openPartGroups[groupNumber] = std::make_pair(std::vector<std::string>(), true);
				lastOpenedPartGroup = groupNumber;
			}
			else if (groupType == "stop") {
				auto groupIt = openPartGroups.find(groupNumber);
				if (groupIt != openPartGroups.end()) {
					if (groupIt->second.second && !groupIt->second.first.empty()) partGroups.push_back(groupIt->second.first);
					openPartGroups.erase(groupIt);
				}
			}
		}
		else if (isXmlElement(line, "group-symbol")) {
			// a group without a symbol is only a logical grouping, so we don't group its staves
			if (lineWithoutSpaces.find(">none<") != std::string::npos) {
				auto groupIt = openPartGroups.find(lastOpenedPartGroup);
				if (groupIt != openPartGroups.end()) groupIt->second.second = false;
			}
		}
		if (isXmlElement(line, "score-part")) {
			std::string partIdStr = xmlAttribute(line, "id");
			for (auto groupIt = openPartGroups.begin(); groupIt != openPartGroups.end(); ++groupIt) {
				groupIt->second.first.push_back(partIdStr);
			}
			if (partIdStr.size() > 1 && partIdStr[0] == 'P' && partIdStr.find_first_not_of("0123456789", 1) == std::string::npos) {
				partID = xmlInt(partIdStr.substr(1), (int)partNumbers.size() + 1);
			}
			else {
				partID = (int)partNumbers.size() + 1;
			}
			partNumbers[partIdStr] = partID;
			// the key is an incrementing index
			// and the pair is a pair of pairs, where the first item is the number of staves of the instrument
			// and the second part is a pair with the part ID and the actual instrument name
			// the default number of staves is 1, hence the 1 below
			// the name is replaced by the one in <part-name>, if there is one
			insts[partID-1] = std::make_pair(1, std::make_pair(partIdStr, "part" + std::to_string(partID)));
			// some software that export MusicXML add sequential indexes to parts, even if a part has more than one staff
			// to remedy this, we make the test below and use the mustMoveKeys boolean for certain actions further down
			if (partID - prevPartID == 1) mustMoveKeys = true;
			else mustMoveKeys = false;
			prevPartID = partID;
		}
		// the name can have attributes (e.g. <part-name print-object="no">) or be empty (<part-name/>)
		else if (isXmlElement(line, "part-name")) {
			ndx1 = lineWithoutSpaces.find(">");
			ndx2 = lineWithoutSpaces.find("</part-name");
			if (ndx1 != std::string::npos && ndx2 != std::string::npos && ndx2 > ndx1 + 1 && insts.find(partID-1) != insts.end()) {
				insts[partID-1].second.second = lineWithoutSpaces.substr(ndx1+1, ndx2-ndx1-1);
			}
		}
		if (startsWith(lineWithoutSpaces, "<partid")) {
			auto partNumberIt = partNumbers.find(xmlAttribute(line, "id"));
			// a part that is not in the part list is ignored
			if (partNumberIt != partNumbers.end()) partID = partNumberIt->second + partIDOffset;
		}
		else if (startsWith(lineWithoutSpaces, "<staves>")) {
			numStaves = xmlInt(lineWithoutSpaces.substr(8), 1);
			if (numStaves > 1) {
				if (mustMoveKeys) {
					// if an instrument has more than one staff
					// and the part IDs for different instruments are sequential,
					// change the keys of the following items in the std::map
					loopIter = (int)insts.size() - 1;
					while (loopIter > partID) {
						if (insts.find(loopIter) != insts.end()) {
							auto nodeHolder = insts.extract(loopIter);
							nodeHolder.key() = loopIter + numStaves - 1;
							insts.insert(std::move(nodeHolder));
						}
						loopIter--;
					}
				}
				insts[partID].first = numStaves;
				insts[partID].second.second += "1";
				for (i = 1; i < numStaves; i++) {
					insts[partID+i] = std::make_pair(numStaves, std::make_pair(insts[partID].second.first, insts[partID].second.second.substr(0, insts[partID].second.second.size()-1)+std::to_string(i+1)));
				}
				if (mustMoveKeys) partIDOffset += (numStaves - 1);
			}
		}
	}
	std::string instLine = "\\insts.init";
	for (auto it = insts.begin(); it != insts.end(); ++it) {
		instLine += " ";
		instLine += it->second.second.second;
	}
	createNewLine(instLine, 0);
	cursorLineIndex++;
	// group the staves of an instrument with more than one staff (e.g. a piano)
	// and the staves of the parts that are grouped in the part list
	// LiveLily connects the staves of a group with lines, and it can group only adjacent staves
	// so each group is written as the staves of consecutive instruments, in the order of insts
	{
		std::vector<std::vector<std::string>> staffGroups;
		// first the groups of parts, as all the staves of the parts in such a group are connected
		for (const auto& group : partGroups) {
			std::vector<std::string> staves;
			for (auto it = insts.begin(); it != insts.end(); ++it) {
				if (std::find(group.begin(), group.end(), it->second.second.first) != group.end()) {
					staves.push_back(it->second.second.second);
				}
			}
			if (staves.size() > 1) staffGroups.push_back(staves);
		}
		// then the staves of each instrument with more than one staff
		// unless they are already included in a group of parts
		for (auto it = insts.begin(); it != insts.end(); ) {
			auto groupEnd = it;
			std::vector<std::string> staves;
			while (groupEnd != insts.end() && groupEnd->second.second.first == it->second.second.first) {
				staves.push_back(groupEnd->second.second.second);
				++groupEnd;
			}
			if (staves.size() > 1) {
				bool included = false;
				for (const auto& group : staffGroups) {
					if (std::find(group.begin(), group.end(), staves[0]) != group.end()) {
						included = true;
						break;
					}
				}
				if (!included) staffGroups.push_back(staves);
			}
			it = groupEnd;
		}
		// write the groups in the order of the staves
		std::sort(staffGroups.begin(), staffGroups.end(), [&](const std::vector<std::string>& a, const std::vector<std::string>& b) {
			auto position = [&](const std::string& name) {
				int pos = 0;
				for (auto it = insts.begin(); it != insts.end(); ++it, ++pos) {
					if (it->second.second.second == name) return pos;
				}
				return pos;
			};
			return position(a[0]) < position(b[0]);
		});
		for (const auto& group : staffGroups) {
			std::string groupLine = "\\group";
			for (const auto& staff : group) groupLine += " \\" + staff;
			createNewLine(groupLine, 0);
			cursorLineIndex++;
		}
	}
	int instNdx = 0;
	int barNum = 0;
	int clefOffset = 0;
	bool isGroupped = false;
	bool clefFollows = false;
	std::map<std::string, int> clefNdxs {{"G", 0}, {"F", 1}, {"C", 2}};
	std::map<int, std::map<int, std::pair<bool, int>>> instClefs;
	// we have read the instruments, we now read the rest of the score to store clef changes
	file.clear();
	file.seekg(0, file.beg);
	while (getline(file, line)) {
		// files with Windows line endings (CRLF) leave a carriage return at the end of each line
		if (!line.empty() && line.back() == '\r') line.pop_back();
		std::string lineWithoutTabs = replaceCharInStr(line, "\t", "");
		std::string lineWithoutSpaces = replaceCharInStr(lineWithoutTabs, " ", "");
		if (startsWith(lineWithoutSpaces, "<partid")) {
			for (auto it = insts.begin(); it != insts.end(); ++it) {
				if (it->second.second.first.compare(xmlAttribute(line, "id")) == 0) {
					// if this insturment is groupped, this is the index of the first instrument of the group
					// the incrementing indexes are found further down when we get a "<staff>" line
					instNdx = it->first;
					// we determine if this instrument is groupped by querying the first value of its pair
					// which is the value to the instrument std::map
					// this value is the number of staves of this instrument
					if (it->second.first > 1) isGroupped = true;
					else isGroupped = false;
					break;
				}
			}
		}
		else if (isXmlElement(line, "measure")) {
			barNum = xmlMeasureNumber(line, barNum);
			if (instClefs.find(instNdx) == instClefs.end()) {
				std::map<int, std::pair<bool, int>> m {{barNum, std::make_pair(false, 0)}};
				instClefs[instNdx] = m;
			}
			//instClefs[instNdx][barNum] = 0;
			if (isGroupped) {
				for (i = 1; i < insts[instNdx].first; i++) {
					instClefs[instNdx+i][barNum] = std::make_pair(false, 0);
				}
			}
		}
		// compare against "<clef" and not "<clef>" because instruments with more than one staves
		// use "<clef number=1>" etc.
		else if (isXmlElement(line, "clef")) {
			clefFollows = true;
			// a clef without a number belongs to the first staff of the part
			clefOffset = xmlInt(xmlAttribute(line, "number"), 1) - 1;
		}
		else if (startsWith(lineWithoutSpaces, "<sign>") && clefFollows) {
			instClefs[instNdx+clefOffset][barNum] = std::make_pair(true, clefNdxs[lineWithoutSpaces.substr(6, lineWithoutSpaces.find_last_of("<")-6)]);
			clefFollows = false;
		}
	}
	// the clef in effect at a bar of a staff, which is the last clef set at or before that bar (treble by default)
	auto clefFor = [&instClefs](int staff, int bar) {
		int clef = 0;
		auto staffIt = instClefs.find(staff);
		if (staffIt == instClefs.end()) return clef;
		for (auto barIt = staffIt->second.begin(); barIt != staffIt->second.end() && barIt->first <= bar; ++barIt) {
			if (barIt->second.first) clef = barIt->second.second;
		}
		return clef;
	};
	file.clear();
	file.seekg(0, file.beg);
	int beatNumerator = 4, beatDenominator = 4;
	std::map<int, std::pair<int, int>> times;
	while (getline(file, line)) {
		// files with Windows line endings (CRLF) leave a carriage return at the end of each line
		if (!line.empty() && line.back() == '\r') line.pop_back();
		std::string lineWithoutTabs = replaceCharInStr(line, "\t", "");
		std::string lineWithoutSpaces = replaceCharInStr(lineWithoutTabs, " ", "");
		if (isXmlElement(line, "measure")) {
			barNum = xmlMeasureNumber(line, barNum);
		}
		else if (startsWith(lineWithoutSpaces, "<beats>")) {
			beatNumerator = xmlInt(lineWithoutSpaces.substr(7), beatNumerator);
		}
		else if (startsWith(lineWithoutSpaces, "<beat-type>")) {
			beatDenominator = xmlInt(lineWithoutSpaces.substr(11), beatDenominator);
			times[barNum] = std::make_pair(beatNumerator, beatDenominator);
		}
	}
	for (int i = 1; i <= barNum; i++) {
		if (times.find(i) == times.end()) {
			times[i] = std::make_pair(beatNumerator, beatDenominator);
		}
		else {
			beatNumerator = times[i].first;
			beatDenominator = times[i].second;
		}
	}
	file.clear();
	file.seekg(0, file.beg);
	// the tempo base is stored as written in LiveLily (e.g. "4", or "4." for a dotted quarter)
	std::string tempoBase = "4";
	int tempo = 120;
	std::map<std::string, std::string> tempoBases {{"whole", "1"}, {"half", "2"}, {"quarter", "4"}, {"eighth", "8"}, {"16th", "16"}, {"32nd", "32"}, {"64th", "64"}};
	std::map<int, std::pair<std::string, int>> tempi;
	// tempi from <sound tempo="..."/>, which are always in quarter notes per minute
	// used for bars that have such a tempo but no metronome marking
	std::map<int, int> soundTempi;
	// the beat unit of the metronome marking being read, without the dots
	std::string beatUnit = "4";
	while (getline(file, line)) {
		// files with Windows line endings (CRLF) leave a carriage return at the end of each line
		if (!line.empty() && line.back() == '\r') line.pop_back();
		std::string lineWithoutTabs = replaceCharInStr(line, "\t", "");
		std::string lineWithoutSpaces = replaceCharInStr(lineWithoutTabs, " ", "");
		if (isXmlElement(line, "measure")) {
			barNum = xmlMeasureNumber(line, barNum);
		}
		// isXmlElement() matches the exact element name, so <beat-unit> doesn't match <beat-unit-dot/>
		else if (isXmlElement(line, "beat-unit")) {
			auto baseIt = tempoBases.find(xmlElementText(line));
			// an unknown beat unit (e.g. "breve") keeps the previous one
			if (baseIt != tempoBases.end()) beatUnit = baseIt->second;
			tempoBase = beatUnit;
		}
		else if (isXmlElement(line, "beat-unit-dot")) {
			// each <beat-unit-dot/> adds a dot to the beat unit right before it
			tempoBase += ".";
		}
		else if (isXmlElement(line, "per-minute")) {
			// only the text of the element is parsed, as the element can have attributes with numbers
			// e.g. <per-minute font-size="12">130</per-minute>, where the first number in the line is the font size
			// the tempo might be written with text, e.g. "c. 120", so we parse the first number in the text
			// and ignore the tempo if there is no number in it
			std::string perMinute = xmlElementText(line);
			size_t firstDigitNdx = perMinute.find_first_of("0123456789");
			if (firstDigitNdx != std::string::npos) {
				tempo = xmlInt(perMinute.substr(firstDigitNdx), tempo);
				tempi[barNum] = std::make_pair(tempoBase, tempo);
			}
		}
		else if (isXmlElement(line, "sound")) {
			std::string soundTempo = xmlAttribute(line, "tempo");
			if (!soundTempo.empty()) {
				// the tempo can have decimals, e.g. tempo="72.5"
				char* end = nullptr;
				double value = strtod(soundTempo.c_str(), &end);
				if (end != soundTempo.c_str() && value >= 1) soundTempi[barNum] = (int)std::lround(value);
			}
		}
	}
	// bars with a playback tempo but without a metronome marking get the playback tempo, in quarter notes
	for (auto it = soundTempi.begin(); it != soundTempi.end(); ++it) {
		if (tempi.find(it->first) == tempi.end()) tempi[it->first] = std::make_pair(std::string("4"), it->second);
	}
	// the tempo of the first bar, which the bars before the first tempo marking get
	if (!tempi.empty()) {
		tempoBase = tempi.begin()->second.first;
		tempo = tempi.begin()->second.second;
	}
	else {
		tempoBase = "4";
		tempo = 120;
	}
	for (int i = 1; i <= barNum; i++) {
		if (tempi.find(i) == tempi.end()) {
			tempi[i] = std::make_pair(tempoBase, tempo);
		}
		else {
			tempoBase = tempi[i].first;
			tempo = tempi[i].second;
		}
	}
	// the staff and voice of each note and <forward>, in the order they appear in the file
	// the <staff> of a note comes after its pitch and type, which the pass below writes as soon as it reads them
	// so we read the staves and voices first, to know which staff each note goes to before writing it
	// a note without a <voice> (usually a chord note) has the voice of the previous note
	std::vector<std::pair<int, int>> eventStaffVoice;
	{
		file.clear();
		file.seekg(0, file.beg);
		bool insideEvent = false;
		int prevVoice = 1;
		while (getline(file, line)) {
			// files with Windows line endings (CRLF) leave a carriage return at the end of each line
			if (!line.empty() && line.back() == '\r') line.pop_back();
			std::string lineWithoutSpaces = replaceCharInStr(replaceCharInStr(line, "\t", ""), " ", "");
			if (isXmlElement(line, "note") || isXmlElement(line, "forward")) {
				eventStaffVoice.push_back(std::make_pair(1, prevVoice));
				insideEvent = true;
			}
			else if (insideEvent && isXmlElement(line, "staff")) {
				eventStaffVoice.back().first = xmlInt(lineWithoutSpaces.substr(lineWithoutSpaces.find('>') + 1), 1);
			}
			else if (insideEvent && isXmlElement(line, "voice")) {
				eventStaffVoice.back().second = xmlInt(lineWithoutSpaces.substr(lineWithoutSpaces.find('>') + 1), prevVoice);
				prevVoice = eventStaffVoice.back().second;
			}
			else if (startsWith(lineWithoutSpaces, "</note>") || startsWith(lineWithoutSpaces, "</forward>")) {
				insideEvent = false;
			}
		}
	}
	size_t eventNdx = 0; // the index of the current note or <forward> in eventStaffVoice
	file.clear();
	file.seekg(0, file.beg);
	// and now we store the rest of the score data
	bool foundNote = false;
	bool foundChordSym = false;
	bool isChord = false;
	bool isRest = false;
	bool firstChordNote = false;
	bool foundDynamic = false;
	bool foundArticulation = false;
	bool tupletStops = false;
	bool areWeInsideTuplet = false;
	bool timeModificationFollows = false;
	bool insideNote = false; // between <note> and </note>
	bool noteHasDuration = false; // whether the current note or rest has a <type>
	// durations in MusicXML are in divisions of a quarter note, set per part with <divisions>
	// we use them to fill staves that don't fill their measure, to write <forward> as rests
	// and to write whole bar rests in any time signature
	long divisions = 1;
	long noteDurationDivs = 0; // the <duration> of the current note
	std::map<int, long> staffPositions; // how much of the measure each staff of the current part has filled so far
	// the voice of each staff of the current part in the current measure
	// a LiveLily line can't have more than one voice, so the notes of other voices on the same staff are not written
	std::map<int, int> staffVoices;
	bool skipForward = false; // a <forward> of a voice that is not written
	bool noteIsChord = false; // whether the current note is part of a chord (only the first note of a chord takes time)
	bool restIsMeasure = false; // a whole bar rest, e.g. <rest measure="yes"/>
	bool skipCurrentNote = false; // grace notes take no time in the measure, so they are not written
	bool insideForward = false;
	long forwardDurationDivs = 0;
	int instNdxOffset = 0;
	lineNum = 1;
	int tupletNumerator = 3, tupletDenominator = 2;
	int alter = 0;
	int octave;
	size_t tupletNdx;
	std::map<std::string, std::string> articulations{{"marcato", "^"}, {"trill", "+"}, {"tenuto", "-"}, {"staccatissimo", "!"},
		{"accent", ">"}, {"staccato", "."}, {"portando", "_"}};
	std::string xmlDurs[7] = {"whole", "half", "quarter", "eighth", "16th", "32nd", "64th"};
	// std::map with instrument index as key and another std::map as value
	// value std::map has the bar number as key, and the Lilypond std::string as value
	std::map<int, std::map<int, std::pair<bool, std::string>>> notes;
	// text in the score (e.g. "accel.") and dynamics are written in a <direction> element before the note they belong to
	// (dynamics can also be written inside the note, in which case they belong to that note)
	// it can't be added to the string of the note right away, because if the note turns out to be the first note of a chord
	// the chord brackets are added around it later, and the text would end up inside the chord
	// so we store it, and add it once its note or chord is complete, i.e. when the next note or rest starts
	// or at the end of the measure or of the staff, e.g. <c' e'>4^"accel."
	// text after the last note of a measure is added to that note
	// notes that need more than three ledger lines are written with \\ottava (which doesn't change their pitch, only how they're displayed)
	// but if a bar needs fewer ottavas with the other clef (treble or bass), that clef is used for the bar instead
	// since the clef of each bar must be known before deciding the ottavas of its notes, the notes are recorded while parsing
	// and the clefs and ottavas are decided once the whole file has been read (see after the parsing loop below)
	struct NoteRecord {
		int staff;
		int bar;
		size_t pos; // the position of the note (or chord) in the string of its bar, where an \\ottava would be inserted
		std::vector<int> pitches; // the positions of its notes in diatonic steps (C0 is 0)
	};
	std::vector<NoteRecord> noteRecords;
	std::vector<int> currentNotePitches; // the positions of the notes of the current note or chord, in diatonic steps (C0 is 0)
	int currentLetterNdx = 0; // c is 0, d is 1, etc.
	size_t noteFirstCharPos = std::string::npos; // the position of the current note (or chord) in the string of its bar
	int noteStaff = 0, noteBar = 0;
	// the number of ledger lines a note (in diatonic steps) needs, given a clef (0 treble, 1 bass, 2 alto)
	auto ledgerLines = [](int step, int clef) {
		// the diatonic steps of the bottom and top lines of the staff: E4 and F5 for treble, G2 and A3 for bass, F3 and G4 for alto
		int bottomLine = 30, topLine = 38;
		if (clef == 1) {
			bottomLine = 18;
			topLine = 26;
		}
		else if (clef == 2) {
			bottomLine = 24;
			topLine = 32;
		}
		if (step < bottomLine) return (bottomLine - step) / 2;
		if (step > topLine) return (step - topLine) / 2;
		return 0;
	};
	// the most ledger lines of the notes of a note or chord, with a clef and an ottava
	auto maxLedgerLines = [&ledgerLines](const std::vector<int>& pitches, int clef, int ottava) {
		int maxLines = 0;
		for (int step : pitches) maxLines = std::max(maxLines, ledgerLines(step - (7 * ottava), clef));
		return maxLines;
	};
	// the current note (or chord) is complete, so we store it, to decide its ottava once the whole file has been read
	auto setOttavaOfCurrentNote = [&]() {
		// rests have no pitches
		if (!currentNotePitches.empty() && noteFirstCharPos != std::string::npos) {
			noteRecords.push_back({noteStaff, noteBar, noteFirstCharPos, currentNotePitches});
		}
		currentNotePitches.clear();
		noteFirstCharPos = std::string::npos;
	};
	std::string textPlacement = "^"; // ^ for above, _ for below
	bool textInThisDirection = false; // to join the texts of a <direction> with more than one <words> into one string
	std::string textForNextNote; // text that was found, waiting for the note it belongs to
	std::string textForCurrentNote; // text of the note (or chord) that is being written
	auto addTextOfCurrentNote = [&]() {
		// the current note is complete, so we can check if it needs an ottava
		setOttavaOfCurrentNote();
		if (!textForCurrentNote.empty()) {
			notes[instNdx+instNdxOffset][barNum].second += orderNoteMarks(textForCurrentNote);
			textForCurrentNote.clear();
		}
	};
	// rests for a duration in divisions, using as few rests as possible (dotted where possible)
	// e.g. " r1." for a whole bar in 6/4, " r1 r4" for 5/4
	auto restsForDuration = [&divisions](long durationDivs) {
		std::string rests;
		if (divisions <= 0 || durationDivs <= 0) return rests;
		// the duration in 64th notes, as there are 16 of them in a quarter note
		long remaining = std::lround((double)durationDivs * 16.0 / (double)divisions);
		const long lengths[13] = {96, 64, 48, 32, 24, 16, 12, 8, 6, 4, 3, 2, 1};
		const std::string names[13] = {"1.", "1", "2.", "2", "4.", "4", "8.", "8", "16.", "16", "32.", "32", "64"};
		while (remaining > 0) {
			for (int k = 0; k < 13; k++) {
				if (lengths[k] <= remaining) {
					rests += " r" + names[k];
					remaining -= lengths[k];
					break;
				}
			}
		}
		return rests;
	};
	// the length of the current measure in divisions, from its time signature
	auto measureLengthDivs = [&]() {
		int beatsNum = 4, beatType = 4;
		auto timeIt = times.find(barNum);
		if (timeIt != times.end() && timeIt->second.second > 0) {
			beatsNum = timeIt->second.first;
			beatType = timeIt->second.second;
		}
		return std::lround((double)beatsNum * 4.0 * (double)divisions / (double)beatType);
	};
	// the number of staves of the current part
	auto numStavesOfPart = [&]() {
		if (!isGroupped) return 1;
		auto instIt = insts.find(instNdx);
		return (instIt == insts.end() ? 1 : std::max(1, instIt->second.first));
	};
	// MusicXML allows a staff to end before its measure does, or to have nothing in it, but LiveLily needs full bars
	// so we fill the rest of the measure of each staff of the current part with rests
	// and write a whole bar rest for a staff that has nothing in this measure
	auto fillStavesToMeasureEnd = [&]() {
		long measureLength = measureLengthDivs();
		for (int staff = 0; staff < numStavesOfPart(); staff++) {
			std::string& staffStr = notes[instNdx+staff][barNum].second;
			if (staffStr.find_first_not_of(' ') == std::string::npos) {
				staffStr += restsForDuration(measureLength);
			}
			else if (staffPositions[staff] < measureLength) {
				staffStr += restsForDuration(measureLength - staffPositions[staff]);
			}
		}
	};
	auto addTextAtEndOfMeasure = [&]() {
		// text after the last note of a measure (or of a staff, in an instrument with many staves) belongs to that note
		// unless the measure has no notes, in which case it waits for the next note
		std::string& measureStr = notes[instNdx+instNdxOffset][barNum].second;
		if (!textForNextNote.empty() && measureStr.find_first_not_of(" ") != std::string::npos) {
			textForCurrentNote += textForNextNote;
			textForNextNote.clear();
		}
		addTextOfCurrentNote();
	};
	auto startNewNote = [&]() {
		// the text found before the note that starts belongs to it
		textForCurrentNote = textForNextNote;
		textForNextNote.clear();
	};
	// dynamics and hairpins are added in the same way as text, so that they are written after a chord, not inside it
	// moves to another staff of the current part, completing the note of the staff we leave
	auto switchToStaff = [&](int newOffset) {
		if (newOffset == instNdxOffset) return;
		addTextOfCurrentNote();
		if (tupletStops) {
			notes[instNdx+instNdxOffset][barNum].second += " }";
			tupletStops = false;
			areWeInsideTuplet = false;
		}
		instNdxOffset = newOffset;
		if (notes[instNdx+instNdxOffset].find(barNum) == notes[instNdx+instNdxOffset].end()) {
			notes[instNdx+instNdxOffset][barNum] = std::make_pair(false, "");
		}
		isChord = false;
		foundNote = false;
	};
	// goes to the staff of the next note or <forward> (from eventStaffVoice)
	// returns false if it's a voice that is not written, because its staff already has another voice in this measure
	auto goToStaffOfNextEvent = [&]() {
		std::pair<int, int> staffVoice = (eventNdx < eventStaffVoice.size() ? eventStaffVoice[eventNdx] : std::make_pair(1, 1));
		eventNdx++;
		int staffOffset = std::max(0, std::min(staffVoice.first - 1, numStavesOfPart() - 1));
		auto voiceIt = staffVoices.find(staffOffset);
		if (voiceIt != staffVoices.end() && voiceIt->second != staffVoice.second) return false;
		staffVoices[staffOffset] = staffVoice.second;
		switchToStaff(staffOffset);
		return true;
	};
	auto addMark = [&](const std::string& mark) {
		if (insideNote) {
			textForCurrentNote += mark;
		}
		else {
			textForNextNote += mark;
			// so the next text is not joined with the text before this mark
			textInThisDirection = false;
		}
	};
	while (getline(file, line)) {
		// files with Windows line endings (CRLF) leave a carriage return at the end of each line
		if (!line.empty() && line.back() == '\r') line.pop_back();
		std::string lineWithoutTabs = replaceCharInStr(line, "\t", "");
		std::string lineWithoutSpaces = replaceCharInStr(lineWithoutTabs, " ", "");
		//std::cout << lineNum << endl;
		// grace notes take no time in the measure, so all their lines are skipped
		if (skipCurrentNote) {
			if (startsWith(lineWithoutSpaces, "</note>")) {
				skipCurrentNote = false;
				insideNote = false;
			}
			lineNum++;
			continue;
		}
		if (startsWith(lineWithoutSpaces, "<partid")) {
			textForNextNote.clear();
			textForCurrentNote.clear();
			for (auto it = insts.begin(); it != insts.end(); ++it) {
				if (it->second.second.first.compare(xmlAttribute(line, "id")) == 0) {
					// if this insturment is groupped, this is the index of the first instrument of the group
					// the incrementing indexes are found further down when we get a "<staff>" line
					instNdx = it->first;
					// we determine if this instrument is groupped by querying the first value of its pair
					// which is the value to the instrument std::map
					// this value is the number of staves of this instrument
					if (it->second.first > 1) isGroupped = true;
					else isGroupped = false;
					break;
				}
			}
		}
		else if (isXmlElement(line, "measure")) {
			instNdxOffset = 0;
			barNum = xmlMeasureNumber(line, barNum);
			if (notes.find(instNdx) == notes.end()) {
				std::map<int, std::pair<bool, std::string>> m;
				notes[instNdx] = m;
			}
			if (notes[instNdx].find(barNum) == notes[instNdx].end()) {
				notes[instNdx][barNum] = std::make_pair(false, "");
			}
			// falsify isChord here because a dynamics symbol might be present before the first note
			// and if isChord is true from the previous bar, the character will be inserted instead of appended, causing a crash
			isChord = false;
			foundNote = false;
			staffPositions.clear();
			staffVoices.clear();
		}
		else if (startsWith(lineWithoutSpaces, "</measure>")) {
			addTextAtEndOfMeasure();
			if (tupletStops) {
				notes[instNdx+instNdxOffset][barNum].second += " }";
				tupletStops = false;
				areWeInsideTuplet = false;
			}
			fillStavesToMeasureEnd();
		}
		// <backup> goes back in time, usually to write the next staff, or another voice
		// the staff of each note is taken from its <staff> element (see goToStaffOfNextEvent()), so there's nothing to do here
		else if (startsWith(lineWithoutSpaces, "</backup>")) {
		}
		// isXmlElement() and not startsWith(), so that <notehead> and <notations> don't match
		else if (isXmlElement(line, "note")) {
			foundNote = true;
			isRest = false;
			insideNote = true;
			noteHasDuration = false;
			noteDurationDivs = 0;
			noteIsChord = false;
			restIsMeasure = false;
			// go to the staff of this note, or skip it if it's a voice that is not written
			// (the lines of a skipped note until </note> are skipped at the beginning of the loop)
			if (!goToStaffOfNextEvent()) {
				skipCurrentNote = true;
				// the text, dynamics and hairpins found before this note belong to it, so they are not written either
				textForNextNote.clear();
			}
		}
		// cue notes are not skipped, as they take time in the measure, like normal notes
		else if (insideNote && isXmlElement(line, "grace")) {
			// the lines of this note until </note> are skipped (see the beginning of the loop)
			skipCurrentNote = true;
		}
		else if (isXmlElement(line, "divisions")) {
			divisions = std::max(1, xmlInt(lineWithoutSpaces.substr(lineWithoutSpaces.find('>') + 1), 1));
		}
		// the duration of a note, or of a <forward>
		else if (isXmlElement(line, "duration")) {
			long durationDivs = xmlInt(lineWithoutSpaces.substr(lineWithoutSpaces.find('>') + 1), 0);
			if (insideNote) noteDurationDivs = durationDivs;
			else if (insideForward) forwardDurationDivs = durationDivs;
		}
		// <forward> skips time in a staff (it's usually an invisible rest), so we write it as rests
		else if (isXmlElement(line, "forward")) {
			insideForward = true;
			forwardDurationDivs = 0;
			skipForward = !goToStaffOfNextEvent();
		}
		else if (startsWith(lineWithoutSpaces, "</forward>")) {
			if (!skipForward) {
				addTextOfCurrentNote();
				if (tupletStops) {
					notes[instNdx+instNdxOffset][barNum].second += " }";
					tupletStops = false;
					areWeInsideTuplet = false;
				}
				notes[instNdx+instNdxOffset][barNum].second += restsForDuration(forwardDurationDivs);
				staffPositions[instNdxOffset] += forwardDurationDivs;
			}
			insideForward = false;
		}
		else if (startsWith(lineWithoutSpaces, "<chord")) {
			foundChordSym = true;
			noteIsChord = true;
		}
		else if (startsWith(lineWithoutSpaces, "<pitch>")) {
			if (foundChordSym) {
				if (!isChord) firstChordNote = true;
				else firstChordNote = false;
				isChord = true;
				foundChordSym = false;
			}
			else {
				isChord = false;
			}
			if (isChord && firstChordNote) {
				// first the last white space, and insert an opening angle bracket after it
				size_t chordInsertNdx = notes[instNdx+instNdxOffset][barNum].second.find_last_of(" ");
				//if (areWeInsideTuplet) chordInsertNdx = notes[instNdx+instNdxOffset][barNum].second.substr(0, chordInsertNdx-1).find_last_of(" ");
				// if we are inside a tuplet, there's an extra white space, before the closing curly bracket
				//if (areWeInsideTuplet) chordInsertNdx = notes[instNdx+instNdxOffset][barNum].second.substr(0, chordInsertNdx).find_last_of(" ");
				notes[instNdx+instNdxOffset][barNum].second.insert(chordInsertNdx+1, "<");
				// then find the index of the last octave symbol
				chordInsertNdx = notes[instNdx+instNdxOffset][barNum].second.find_last_of("'");
				if (chordInsertNdx == std::string::npos || chordInsertNdx < findChordStart(notes[instNdx+instNdxOffset][barNum].second)) {
					chordInsertNdx = notes[instNdx+instNdxOffset][barNum].second.find_last_of(",");
					// if there are no octave symbols, then the closing angle bracket goes right after the note character
					if (chordInsertNdx == std::string::npos || chordInsertNdx < findChordStart(notes[instNdx+instNdxOffset][barNum].second)) {
						// we search for the last white space and add 3, one for the white space, one for the opening angle bracket
						// and one for the note character
						chordInsertNdx = notes[instNdx+instNdxOffset][barNum].second.find_last_of(" ") + 3;
						if (notes[instNdx+instNdxOffset][barNum].second.size() >= chordInsertNdx + 2) {
							// if there is an accidental concatenated to the note
							if (notes[instNdx+instNdxOffset][barNum].second.substr(chordInsertNdx, 2) == "es" || \
									notes[instNdx+instNdxOffset][barNum].second.substr(chordInsertNdx, 2) == "is" || \
									notes[instNdx+instNdxOffset][barNum].second.substr(chordInsertNdx, 2) == "eh" || \
									notes[instNdx+instNdxOffset][barNum].second.substr(chordInsertNdx, 2) == "ih") {
								chordInsertNdx += 2;
								if (notes[instNdx+instNdxOffset][barNum].second.size() >= chordInsertNdx + 2) {
									// if there is an additional accidental concatenated to the note
									if (notes[instNdx+instNdxOffset][barNum].second.substr(chordInsertNdx, 2) == "es" || \
											notes[instNdx+instNdxOffset][barNum].second.substr(chordInsertNdx, 2) == "is" || \
											notes[instNdx+instNdxOffset][barNum].second.substr(chordInsertNdx, 2) == "eh" || \
											notes[instNdx+instNdxOffset][barNum].second.substr(chordInsertNdx, 2) == "ih") {
										chordInsertNdx += 2;
									}
								}
							}
						}
					}
					else {
						chordInsertNdx += 1;
					}
				}
				else {
					chordInsertNdx += 1;
				}
				chordInsertNdx = std::min(chordInsertNdx, notes[instNdx+instNdxOffset][barNum].second.size());
				notes[instNdx+instNdxOffset][barNum].second.insert(chordInsertNdx, ">");
			}
		}
		else if (startsWith(lineWithoutSpaces, "</note>")) {
			// whole bar rests (<rest measure="yes"/>) usually have no <type>, and if they have one, it's a whole note in any time signature
			// so we write them from their duration, which fills the bar (e.g. r1. in 6/4)
			if (isRest && !noteHasDuration) {
				std::string& measureStr = notes[instNdx+instNdxOffset][barNum].second;
				std::string rests = restsForDuration(noteDurationDivs);
				if (!rests.empty() && endsWith(measureStr, " r")) {
					measureStr = measureStr.substr(0, measureStr.size() - 2) + rests;
				}
				else {
					measureStr += "1";
				}
			}
			// only the first note of a chord takes time
			if (!noteIsChord) staffPositions[instNdxOffset] += noteDurationDivs;
			insideNote = false;
		}
		else if (startsWith(lineWithoutSpaces, "<step>")) {
			// got idea for converting the case from upper to lower from
			// https://www.geeksforgeeks.org/case-conversion-lower-upper-vice-versa-std::string-using-bitwise-operators-cc/
			const char c = (lineWithoutSpaces.substr(6, 1)[0] | 32);
			size_t letterNdx = std::string("cdefgab").find(c);
			currentLetterNdx = (letterNdx == std::string::npos ? 0 : (int)letterNdx);
			if (!isChord) {
				addTextOfCurrentNote();
				if (tupletStops) {
					notes[instNdx+instNdxOffset][barNum].second += " }";
					tupletStops = false;
					areWeInsideTuplet = false;
				}
				notes[instNdx+instNdxOffset][barNum].second += " ";
				// the position of the note, where an \\ottava might be inserted once the note (or chord) is complete
				noteFirstCharPos = notes[instNdx+instNdxOffset][barNum].second.size();
				noteStaff = instNdx + instNdxOffset;
				noteBar = barNum;
				notes[instNdx+instNdxOffset][barNum].second += c;
				startNewNote();
			}
			else {
				size_t noteInsertNdx = findChordEnd(notes[instNdx+instNdxOffset][barNum].second);
				notes[instNdx+instNdxOffset][barNum].second.insert(noteInsertNdx, " ");
				noteInsertNdx++;
				notes[instNdx+instNdxOffset][barNum].second = notes[instNdx+instNdxOffset][barNum].second.substr(0, noteInsertNdx) + c + notes[instNdx+instNdxOffset][barNum].second.substr(noteInsertNdx);
			}
		}
		else if (startsWith(lineWithoutSpaces, "<alter>")) {
			alter = xmlInt(lineWithoutSpaces.substr(7), 0);
			size_t alterInsertNdx;
			switch (alter) {
				case -1:
					if (!isChord) {
						notes[instNdx+instNdxOffset][barNum].second += "es";
					}
					else {
						alterInsertNdx = findChordEnd(notes[instNdx+instNdxOffset][barNum].second);
						notes[instNdx+instNdxOffset][barNum].second.insert(alterInsertNdx, "es");
					}
					break;
				case 1:
					if (!isChord) {
						notes[instNdx+instNdxOffset][barNum].second += "is";
					}
					else {
						alterInsertNdx = findChordEnd(notes[instNdx+instNdxOffset][barNum].second);
						notes[instNdx+instNdxOffset][barNum].second.insert(alterInsertNdx, "is");
					}
					break;
				default:
					break;
			}
			// reset alter for safety
			alter = 0;
		}
		// time modification concerns tuplets
		else if (startsWith(lineWithoutSpaces, "<time-modification>")) {
			timeModificationFollows = true;
		}
		else if (startsWith(lineWithoutSpaces, "</time-modification>")) {
			timeModificationFollows = false;
		}
		else if (startsWith(lineWithoutSpaces, "<actual-notes>") && timeModificationFollows && !areWeInsideTuplet) {
			tupletNumerator = xmlInt(lineWithoutSpaces.substr(14), 3);
		}
		else if (startsWith(lineWithoutSpaces, "<normal-notes>") && timeModificationFollows && !areWeInsideTuplet) {
			tupletDenominator = xmlInt(lineWithoutSpaces.substr(14), 2);
		}
		else if (startsWith(lineWithoutSpaces, "<tuplet")) {
			tupletNdx = lineWithoutSpaces.find("type");
			if (lineWithoutSpaces.substr(tupletNdx+6, lineWithoutSpaces.substr(tupletNdx+6).find("\"")).compare("start") == 0 && !areWeInsideTuplet) {
				// first the last white space, and insert an opening angle bracket after it
				tupletNdx = notes[instNdx+instNdxOffset][barNum].second.find_last_of(" ");
				std::string tupletStr = "\\tuplet "+std::to_string(tupletNumerator)+(std::string)"/"+std::to_string(tupletDenominator)+(std::string)" { ";
				notes[instNdx+instNdxOffset][barNum].second.insert(tupletNdx+1, tupletStr);
				// an \\ottava for the current note goes after the opening of the tuplet
				if (noteFirstCharPos != std::string::npos && tupletNdx+1 <= noteFirstCharPos) noteFirstCharPos += tupletStr.size();
				areWeInsideTuplet = true;
			}
			else if (lineWithoutSpaces.substr(tupletNdx+6, lineWithoutSpaces.substr(tupletNdx+6).find("\"")).compare("stop") == 0) {
				tupletStops = true;
			}
		}
		else if (startsWith(lineWithoutSpaces, "<octave>")) {
			octave = xmlInt(lineWithoutSpaces.substr(8), 4);
			// the position of this note (of the current note or chord) in diatonic steps
			currentNotePitches.push_back((octave * 7) + currentLetterNdx);
			if (octave > 3) {
				for (i = 3; i < octave; i++) {
					if (!isChord) {
						notes[instNdx+instNdxOffset][barNum].second += "'";
					}
					else {
						size_t octaveInsertNdx = findChordEnd(notes[instNdx+instNdxOffset][barNum].second);
						notes[instNdx+instNdxOffset][barNum].second.insert(octaveInsertNdx, "'");
					}
				}
			}
			else if (octave < 3) {
				for (i = 3; i > octave; i--) {
					if (!isChord) {
						notes[instNdx+instNdxOffset][barNum].second += ",";
					}
					else {
						size_t octaveInsertNdx = findChordEnd(notes[instNdx+instNdxOffset][barNum].second);
						notes[instNdx+instNdxOffset][barNum].second.insert(octaveInsertNdx, ",");
					}
				}
			}
		}
		else if (startsWith(lineWithoutSpaces, "<rest")) {
			addTextOfCurrentNote();
			if (tupletStops) {
				notes[instNdx+instNdxOffset][barNum].second += " }";
				tupletStops = false;
				areWeInsideTuplet = false;
			}
			notes[instNdx+instNdxOffset][barNum].second += " r";
			startNewNote();
			isRest = true;
			restIsMeasure = (xmlAttribute(line, "measure") == "yes");
			if (isChord) isChord = false;
		}
		// isXmlElement(), as <type> can have attributes, e.g. <type size="full">half</type>
		else if (isXmlElement(line, "type")) {
			std::string typeStr = lineWithoutSpaces.substr(lineWithoutSpaces.find('>') + 1);
			typeStr = typeStr.substr(0, typeStr.find('<'));
			for (i = 0; i < 7; i++) {
				// the duration of a whole bar rest is written from its <duration> (see </note>), not from its type
				// as its type is usually whole in any time signature
				// some files mark it with measure="yes", others only give it the duration of the whole measure
				if (isRest && (restIsMeasure || noteDurationDivs == measureLengthDivs())) break;
				if (typeStr.compare(xmlDurs[i]) == 0) {
					if (!isChord) {
						notes[instNdx+instNdxOffset][barNum].second += std::to_string((int)pow(2, i));
					}
					noteHasDuration = true;
					break;
				}
			}
		}
		// the placement of text, which is set for each <direction> (above if it's not set)
		else if (isXmlElement(line, "direction")) {
			textPlacement = (xmlAttribute(line, "placement") == "below" ? "_" : "^");
			textInThisDirection = false;
		}
		// the text itself, e.g. <words font-style="italic">accel.</words>
		// we use line and not lineWithoutSpaces, because we want the spaces to be included in the text
		else if (isXmlElement(line, "words")) {
			size_t textStart = line.find('>');
			// an empty element (<words/>) has no text
			if (textStart != std::string::npos && line[textStart-1] != '/') {
				size_t textEnd = line.find("</words", textStart);
				std::string text = line.substr(textStart+1, (textEnd == std::string::npos ? std::string::npos : textEnd - textStart - 1));
				text = xmlText(text);
				if (!text.empty()) {
					if (textInThisDirection && !textForNextNote.empty()) {
						// insert the text before the closing quote of the text of this direction
						textForNextNote.insert(textForNextNote.size()-1, " " + text);
					}
					else {
						textForNextNote += textPlacement + "\"" + text + "\"";
					}
					textInThisDirection = true;
				}
			}
		}
		else if (startsWith(lineWithoutSpaces, "<dot")) {
			// text is added after the note is complete, so the dot can always be appended
			if (!isChord) {
				notes[instNdx+instNdxOffset][barNum].second += ".";
			}
		}
		else if (startsWith(lineWithoutSpaces, "<tiedtype")) {
			if (lineWithoutSpaces.find("start") != std::string::npos) {
				if (!isChord) notes[instNdx+instNdxOffset][barNum].second += "~";
				notes[instNdx+instNdxOffset][barNum].first = true;
			}
		}
		else if (foundDynamic) {
			// a dynamic is an empty element, e.g. <f/>, which we write as \f
			// other elements, like <other-dynamics>text</other-dynamics>, are ignored
			if (lineWithoutSpaces.size() > 3 && startsWith(lineWithoutSpaces, "<") && endsWith(lineWithoutSpaces, "/>")) {
				std::string dynamicName = lineWithoutSpaces.substr(1, lineWithoutSpaces.size()-3);
				if (!dynamicName.empty() && std::all_of(dynamicName.begin(), dynamicName.end(), ::isalpha)) {
					addMark("\\" + dynamicName);
				}
			}
			foundDynamic = false;
		}
		// <dymanics come before the actual dynamic which is parsed in the else if above
		// by assigning true to foundDynamic after the parsing chunk, we ensure that this will work properly
		else if (startsWith(lineWithoutSpaces, "<dynamics")) {
			foundDynamic = true;
		}
		// hairpins
		else if (isXmlElement(line, "wedge")) {
			std::string wedgeType = xmlAttribute(line, "type");
			if (wedgeType == "crescendo") {
				addMark("\\<");
			}
			else if (wedgeType == "diminuendo") {
				addMark("\\>");
			}
			else if (wedgeType == "stop") {
				addMark("\\!");
				// a hairpin that stops after the first note of a bar started in a previous bar
				if (foundNote) notes[instNdx+instNdxOffset][barNum].first = true; // this is a linked bar
			}
		}
		else if (startsWith(lineWithoutSpaces, "</articulations")) {
			foundArticulation = false;
		}
		else if (foundArticulation) {
			if (articulations.find(lineWithoutSpaces.substr(1, lineWithoutSpaces.substr(1).find("/"))) != articulations.end()) {
				if (!isChord) {
					notes[instNdx+instNdxOffset][barNum].second += "-";
					notes[instNdx+instNdxOffset][barNum].second += articulations[lineWithoutSpaces.substr(1, lineWithoutSpaces.substr(1).find("/"))];
				}
			}
		}
		else if (startsWith(lineWithoutSpaces, "<articulations")) {
			if (!isRest) foundArticulation = true;
		}
		lineNum++;
	}
	// the text of the last note, in case the file doesn't end with </measure>
	addTextAtEndOfMeasure();
	// in case we have a tuplet at the very end of the score
	if (tupletStops) {
		notes[instNdx+instNdxOffset][barNum].second += " }";
	}
	// choose the clef of each bar of each staff, and the ottavas of its notes
	// this must happen before the hairpins are moved below, as the positions of the notes in the strings are stored
	// and moving the hairpins changes the strings before some notes
	// first, for each bar, check if the other clef (treble or bass) needs fewer ottavas than the clef that is set
	std::map<int, std::map<int, int>> chosenClefs; // staff, bar, clef
	{
		// the number of notes of a bar that need more than three ledger lines without an ottava, with a clef
		std::map<std::pair<int, int>, std::vector<const NoteRecord*>> notesOfBars;
		for (const NoteRecord& record : noteRecords) notesOfBars[std::make_pair(record.staff, record.bar)].push_back(&record);
		for (auto& staffBar : notesOfBars) {
			int staff = staffBar.first.first, bar = staffBar.first.second;
			int clef = clefFor(staff, bar);
			// only treble and bass are switched, alto and percussion staves keep their clef
			if (clef != 0 && clef != 1) continue;
			int otherClef = (clef == 0 ? 1 : 0);
			int needOttava = 0, needOttavaWithOtherClef = 0;
			for (const NoteRecord* record : staffBar.second) {
				if (maxLedgerLines(record->pitches, clef, 0) > 3) needOttava++;
				if (maxLedgerLines(record->pitches, otherClef, 0) > 3) needOttavaWithOtherClef++;
			}
			if (needOttavaWithOtherClef < needOttava) chosenClefs[staff][bar] = otherClef;
		}
	}
	// the clef written for a bar: the chosen one, or the one in effect
	auto clefOfBar = [&chosenClefs, &clefFor](int staff, int bar) {
		auto staffIt = chosenClefs.find(staff);
		if (staffIt != chosenClefs.end()) {
			auto barIt = staffIt->second.find(bar);
			if (barIt != staffIt->second.end()) return barIt->second;
		}
		return clefFor(staff, bar);
	};
	// then decide the ottavas, note by note, in the order they were parsed (which is their order in each staff)
	{
		std::map<int, int> staffOttava; // the current ottava of each staff
		std::map<int, int> staffOttavaBar; // the bar of the last note of each staff that was checked
		// the ottavas to insert in each bar of each staff, as positions and strings
		std::map<std::pair<int, int>, std::vector<std::pair<size_t, std::string>>> insertions;
		for (const NoteRecord& record : noteRecords) {
			int clef = clefOfBar(record.staff, record.bar);
			// percussion staves don't need an ottava
			if (clef == 3) continue;
			// LiveLily resets the ottava at the start of every bar, so the first note of a bar is decided anew
			// (as if no ottava was active), and the following notes keep the ottava as long as it fits them
			bool newBar = (staffOttavaBar.find(record.staff) == staffOttavaBar.end() || staffOttavaBar[record.staff] != record.bar);
			int currentOttava = ((newBar || staffOttava.find(record.staff) == staffOttava.end()) ? 0 : staffOttava[record.staff]);
			int ottava = currentOttava;
			if (maxLedgerLines(record.pitches, clef, currentOttava) <= 3) {
				// keep the current ottava, unless the note is back inside the staff without it
				// so that notes at the edge of the limit don't change the ottava back and forth
				if (currentOttava != 0 && maxLedgerLines(record.pitches, clef, 0) == 0) ottava = 0;
			}
			else {
				// find the ottava closest to none that keeps the note within three ledger lines
				// or the one with the fewest ledger lines if none of them does (e.g. a chord that is too wide)
				const int candidates[5] = {0, 1, -1, 2, -2};
				int fewestLines = std::numeric_limits<int>::max();
				for (int candidate : candidates) {
					int lines = maxLedgerLines(record.pitches, clef, candidate);
					if (lines <= 3) {
						ottava = candidate;
						break;
					}
					if (lines < fewestLines) {
						fewestLines = lines;
						ottava = candidate;
					}
				}
			}
			// write the ottava if it's different than the one in effect (at the start of a bar, no ottava is in effect)
			if (ottava != currentOttava) {
				insertions[std::make_pair(record.staff, record.bar)].push_back(std::make_pair(record.pos, "\\ottava " + std::to_string(ottava) + " "));
			}
			staffOttava[record.staff] = ottava;
			staffOttavaBar[record.staff] = record.bar;
		}
		// insert from the last position to the first, so that each insertion doesn't move the positions before it
		for (auto& staffBar : insertions) {
			std::string& barStr = notes[staffBar.first.first][staffBar.first.second].second;
			for (auto it = staffBar.second.rbegin(); it != staffBar.second.rend(); ++it) {
				if (it->first <= barStr.size()) barStr.insert(it->first, it->second);
			}
		}
	}
	// LiveLily handles each bar separately, so a hairpin (\\< or \\>) must start and end in the same bar
	// but in a MusicXML file a hairpin can continue over the next bars, so here we split such hairpins at the bar lines
	// each bar the hairpin spans gets its own part of it, as long as the hairpin covers at least two of its notes
	// if there are enough dynamics between the dynamic where the hairpin starts and the one where it ends
	// the part of each bar ends at its last note with the next of these dynamics (a dynamic ends a hairpin in LiveLily)
	// and the part of the next bar starts at its first note with this dynamic
	// e.g. p\\< over three bars to f becomes: bar 1: p\\< ... mp, bar 2: mp\\< ... mf, bar 3: mf\\< ... f
	// if there are fewer dynamics in between than bars, they are spread evenly, and the bars where the dynamic doesn't change get no hairpin
	// if the two dynamics are adjacent (e.g. f and mf), or one of them is not known, the hairpin ends at the end of its first part
	static const std::vector<std::string> dynamicNames = {"ppp", "pp", "p", "mp", "mf", "f", "ff", "fff"};
	struct NoteToken {
		size_t start;
		size_t end;
	};
	struct HairpinMark {
		size_t pos;
		char kind; // '<' and '>' for starts, '!' for stops, 'd' for dynamics
		int level; // the index of a dynamic in dynamicNames, or -1
		int token; // the index of the note it belongs to
	};
	// the notes (and rests and chords) of a bar, as positions in its string, skipping tuplet openings and closings, and ottavas
	auto noteTokensOf = [](const std::string& str) {
		std::vector<NoteToken> tokens;
		size_t pos = 0;
		while (pos < str.size()) {
			pos = str.find_first_not_of(' ', pos);
			if (pos == std::string::npos) break;
			if (str.compare(pos, 7, "\\tuplet") == 0) {
				size_t braceNdx = str.find('{', pos);
				pos = (braceNdx == std::string::npos ? str.size() : braceNdx + 1);
				continue;
			}
			if (str.compare(pos, 7, "\\ottava") == 0) {
				size_t argStart = str.find_first_not_of(' ', pos + 7);
				if (argStart == std::string::npos) break;
				size_t argEnd = str.find(' ', argStart);
				pos = (argEnd == std::string::npos ? str.size() : argEnd);
				continue;
			}
			if (str[pos] == '}') {
				pos++;
				continue;
			}
			// a note ends at the first white space outside a chord and outside text
			bool insideChord = (str[pos] == '<');
			bool insideText = false;
			size_t end;
			for (end = pos + (insideChord ? 1 : 0); end < str.size(); end++) {
				char c = str[end];
				if (c == '"') insideText = !insideText;
				// a '>' after a backslash or a hyphen is a diminuendo or an accent, not the end of a chord
				else if (!insideText && insideChord && c == '>' && str[end-1] != '\\' && str[end-1] != '-') insideChord = false;
				else if (!insideText && !insideChord && c == ' ') break;
			}
			tokens.push_back({pos, end});
			pos = end;
		}
		return tokens;
	};
	// the hairpin marks and dynamics of a bar, in order, ignoring text
	auto hairpinMarksOf = [](const std::string& str, const std::vector<NoteToken>& tokens) {
		std::vector<HairpinMark> marks;
		for (int t = 0; t < (int)tokens.size(); t++) {
			bool insideText = false;
			for (size_t i = tokens[t].start; i + 1 < tokens[t].end; i++) {
				char c = str[i];
				if (c == '"') {
					insideText = !insideText;
					continue;
				}
				if (insideText || c != '\\') continue;
				char next = str[i+1];
				if (next == '<' || next == '>' || next == '!') {
					marks.push_back({i, next, -1, t});
					i++;
				}
				else if (next == 'p' || next == 'm' || next == 'f') {
					size_t nameEnd = i + 1;
					while (nameEnd < tokens[t].end && isalpha(str[nameEnd])) nameEnd++;
					auto nameIt = std::find(dynamicNames.begin(), dynamicNames.end(), str.substr(i + 1, nameEnd - i - 1));
					marks.push_back({i, 'd', (nameIt == dynamicNames.end() ? -1 : (int)(nameIt - dynamicNames.begin())), t});
					i = nameEnd - 1;
				}
			}
		}
		return marks;
	};
	for (auto& staff : notes) {
		std::vector<std::string*> barStrs;
		for (auto& bar : staff.second) barStrs.push_back(&bar.second.second);
		int currentLevel = -1; // the dynamic in effect, as an index in dynamicNames
		for (size_t a = 0; a < barStrs.size(); a++) {
			std::vector<NoteToken> tokens = noteTokensOf(*barStrs[a]);
			std::vector<HairpinMark> marks = hairpinMarksOf(*barStrs[a], tokens);
			for (size_t mi = 0; mi < marks.size(); mi++) {
				const HairpinMark& mark = marks[mi];
				if (mark.kind == 'd') {
					if (mark.level > -1) currentLevel = mark.level;
					continue;
				}
				// a hairpin ends at the next stop, dynamic, or start, so if there is any mark after it in its bar, it ends in its bar
				if ((mark.kind != '<' && mark.kind != '>') || mi + 1 < marks.size()) continue;
				// this hairpin continues after the end of its bar, so find where it ends, and with which dynamic
				char dir = mark.kind;
				int startLevel = currentLevel;
				int endLevel = -1;
				size_t z = barStrs.size() - 1; // the bar where it ends
				int closeToken = -1; // the note where it ends
				bool closed = false;
				for (size_t b = a + 1; b < barStrs.size() && !closed; b++) {
					std::vector<NoteToken> barTokens = noteTokensOf(*barStrs[b]);
					std::vector<HairpinMark> barMarks = hairpinMarksOf(*barStrs[b], barTokens);
					if (barMarks.empty()) continue;
					closed = true;
					z = b;
					closeToken = barMarks[0].token;
					if (barMarks[0].kind == 'd') {
						endLevel = barMarks[0].level;
					}
					// a stop with a dynamic on the same note
					else if (barMarks[0].kind == '!' && barMarks.size() > 1 && barMarks[1].kind == 'd' && barMarks[1].token == barMarks[0].token) {
						endLevel = barMarks[1].level;
					}
				}
				// the parts of the hairpin, in the bars where it covers at least two notes
				struct HairpinPart {
					size_t bar;
					int firstToken;
					int lastToken;
				};
				std::vector<HairpinPart> parts;
				std::map<size_t, std::vector<NoteToken>> tokensOfBars;
				for (size_t b = a; b <= z; b++) {
					tokensOfBars[b] = (b == a ? tokens : noteTokensOf(*barStrs[b]));
					if (tokensOfBars[b].empty()) continue;
					int first = (b == a ? mark.token : 0);
					int last = ((b == z && closed) ? closeToken : (int)tokensOfBars[b].size() - 1);
					if (last > first) parts.push_back({b, first, last});
				}
				// the changes to make to the bars, as positions with the number of characters to erase and the string to insert
				std::map<size_t, std::vector<std::pair<size_t, std::pair<size_t, std::string>>>> edits;
				auto tokenHasDynamic = [&](size_t b, int t) {
					std::vector<HairpinMark> barMarks = hairpinMarksOf(*barStrs[b], tokensOfBars[b]);
					for (const HairpinMark& m : barMarks) if (m.token == t && m.kind == 'd') return true;
					return false;
				};
				// a hairpin start or stop goes at the end of the note, after its other marks
				auto addAtEndOfNote = [&](size_t b, int t, const std::string& str) {
					edits[b].push_back(std::make_pair(tokensOfBars[b][t].end, std::make_pair((size_t)0, str)));
				};
				// a dynamic that ends a hairpin goes before any hairpin start of the same note, as a dynamic after a start would end it
				auto addDynamicToNote = [&](size_t b, int t, int level) {
					if (tokenHasDynamic(b, t)) return; // the dynamic of the note ends the hairpin
					size_t pos = tokensOfBars[b][t].end;
					for (const HairpinMark& m : hairpinMarksOf(*barStrs[b], tokensOfBars[b])) {
						if (m.token == t && (m.kind == '<' || m.kind == '>')) {
							pos = m.pos;
							break;
						}
					}
					edits[b].push_back(std::make_pair(pos, std::make_pair((size_t)0, "\\" + dynamicNames[level])));
				};
				std::string dirMark = std::string("\\") + dir;
				bool keepOriginalStart = false;
				int distance = endLevel - startLevel;
				bool levelsKnown = (startLevel > -1 && endLevel > -1);
				bool rightDirection = (dir == '<' ? distance > 0 : distance < 0);
				if (parts.empty()) {
					// the hairpin covers less than two notes in every bar, so it's removed
				}
				else if (!levelsKnown || !rightDirection || std::abs(distance) <= 1) {
					// there is no dynamic in between, so the hairpin ends at the end of its first part
					const HairpinPart& part = parts[0];
					if (part.bar == a) keepOriginalStart = true;
					else addAtEndOfNote(part.bar, part.firstToken, dirMark);
					if (!(part.bar == z && closed)) addAtEndOfNote(part.bar, part.lastToken, "\\!");
				}
				else {
					// spread the dynamics between the start and end dynamics over the parts
					int numParts = (int)parts.size();
					int steps = std::abs(distance);
					int sign = (distance > 0 ? 1 : -1);
					std::vector<int> levels(numParts + 1);
					levels[0] = startLevel;
					levels[numParts] = endLevel;
					for (int j = 1; j < numParts; j++) {
						// rounded to the nearest dynamic, so that the dynamics are spread evenly and the end dynamic is reached where the hairpin ends
						levels[j] = startLevel + (sign * (int)std::floor(((double)(steps * j) / (double)numParts) + 0.5));
					}
					for (int j = 0; j < numParts; j++) {
						const HairpinPart& part = parts[j];
						// no change of dynamic in this part, so no hairpin
						if (levels[j+1] == levels[j]) continue;
						if (j == 0 && part.bar == a) {
							keepOriginalStart = true;
						}
						else {
							// a part after the first starts with the dynamic the previous part ended with
							// the dynamic goes before the start of the hairpin, as a dynamic after it would end it
							std::string startStr = dirMark;
							if (j > 0 && !tokenHasDynamic(part.bar, part.firstToken)) startStr = "\\" + dynamicNames[levels[j]] + dirMark;
							addAtEndOfNote(part.bar, part.firstToken, startStr);
						}
						// the part ends with the next dynamic, unless it ends where the hairpin ends in the file
						// the last part, if the hairpin ends on the first note of the next bar, ends with a stop
						// and the end dynamic is written on that first note, as in the file
						if (!(part.bar == z && closed)) {
							if (j == numParts - 1 && closed) addAtEndOfNote(part.bar, part.lastToken, "\\!");
							else addDynamicToNote(part.bar, part.lastToken, levels[j+1]);
						}
					}
				}
				if (!keepOriginalStart) edits[a].push_back(std::make_pair(mark.pos, std::make_pair((size_t)2, std::string(""))));
				// apply the changes of each bar from the last position to the first, so that the positions before them don't change
				for (auto& barEdits : edits) {
					std::stable_sort(barEdits.second.begin(), barEdits.second.end(),
							[](const std::pair<size_t, std::pair<size_t, std::string>>& x, const std::pair<size_t, std::pair<size_t, std::string>>& y) {
								return x.first > y.first;
							});
					std::string& barStr = *barStrs[barEdits.first];
					for (auto& edit : barEdits.second) {
						if (edit.first > barStr.size()) continue;
						barStr.erase(edit.first, edit.second.first);
						barStr.insert(edit.first, edit.second.second);
					}
				}
				// the hairpin was the last mark of this bar, so we move on to the next bar
				break;
			}
		}
	}
	// a hairpin stop (\\!) without an open hairpin gives an error in LiveLily, so it is removed
	// (e.g. the stop of a hairpin that was split above, or whose start was in a voice that is not written)
	// a dynamic ends an open hairpin, and a start while a hairpin is open ends the open one and starts a new one
	for (auto& staff : notes) {
		for (auto& bar : staff.second) {
			std::string& barStr = bar.second.second;
			bool hairpinOpen = false;
			bool insideText = false;
			for (size_t i = 0; i + 1 < barStr.size(); i++) {
				if (barStr[i] == '"') {
					insideText = !insideText;
					continue;
				}
				if (insideText || barStr[i] != '\\') continue;
				char next = barStr[i+1];
				if (next == '<' || next == '>') {
					hairpinOpen = true;
				}
				else if (next == '!') {
					if (hairpinOpen) {
						hairpinOpen = false;
					}
					else {
						barStr.erase(i, 2);
						i--;
					}
				}
				// dynamics, e.g. \\p, \\mf, \\ff
				else if (next == 'p' || next == 'm' || next == 'f') {
					hairpinOpen = false;
				}
			}
		}
	}
	// check if we have a zero tempo, which might be the case with a first, incomplete bar, or a tempo written as 0
	// and replace it with the closest non-zero value of the following bars, or the previous bars, or a default value
	// (the tempo base is always valid, as an unknown beat unit keeps the previous one)
	for (auto it = tempi.begin(); it != tempi.end(); ++it) {
		int& value = it->second.second;
		if (value > 0) continue;
		int replacement = 120;
		bool found = false;
		for (auto next = std::next(it); next != tempi.end() && !found; ++next) {
			if (next->second.second > 0) {
				replacement = next->second.second;
				found = true;
			}
		}
		// a reverse iterator made from it points to the element before it
		for (auto prev = std::make_reverse_iterator(it); prev != tempi.rend() && !found; ++prev) {
			if (prev->second.second > 0) {
				replacement = prev->second.second;
				found = true;
			}
		}
		value = replacement;
	}
	// incomplete bars might not contain a time signature at all
	// in this case, we check if the notes map is beigger than the tempi map
	// and it it is, we create a new entry with the first key of the notes map
	// and the values of the first tempi item
	// if there is no tempo at all, we use the defaults
	if (tempi.empty() && !notes[0].empty()) {
		tempi[notes[0].begin()->first] = std::make_pair(tempoBase, tempo);
	}
	else if (notes[0].size() > tempi.size()) {
		auto notesIt = notes[0].begin();
		auto tempiIt = tempi.begin();
		tempi[notesIt->first] = std::make_pair(tempiIt->second.first, tempiIt->second.second);
	}
	// now write the notes in LiveLily notation
	createNewLine("", 0);
	cursorLineIndex++;
	std::map<int, std::string> clefSymbols {{0, "treble"}, {1, "bass"}, {2, "alto"}};
	// all parts have the same number of bars, so we query the first one
	// to determine how many bars we have to write
	for (auto it = notes[0].begin(); it != notes[0].end(); ++it) {
		std::string barOpening = "\\bar " + std::to_string(it->first) + " {";
		createNewLine(barOpening, 0);
		cursorLineIndex++;
		std::string time = tabStr + "\\time " + std::to_string(times[it->first].first) + "/" + std::to_string(times[it->first].second);
		createNewLine(time, 0);
		cursorLineIndex++;
		std::string tempoStr = tabStr + "\\tempo " + tempi[it->first].first + " = " + std::to_string(tempi[it->first].second);
		createNewLine(tempoStr, 0);
		cursorLineIndex++;
		std::string beatAtStr = tabStr + "\\beatat " + tempi[it->first].first;
		createNewLine(beatAtStr, 0);
		cursorLineIndex++;
		for (auto it2 = insts.begin(); it2 != insts.end(); ++it2) {
			std::string instLine = tabStr + "\\" + it2->second.second.second;
			// the clef in effect at this bar, and not the last clef of the staff in the whole score
			instLine += " \\clef ";
			// the clef chosen for this bar (to avoid ottavas), or the one in effect
			instLine += clefSymbols[clefOfBar(it2->first, it->first)];
			instLine += notes[it2->first][it->first].second;
			createNewLine(instLine, 0);
			cursorLineIndex++;
		}
		createNewLine("}", 0);
		cursorLineIndex++;
		createNewLine("", 0);
		cursorLineIndex++;
	}
}
