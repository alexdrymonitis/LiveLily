#ifndef MUSICXML_H
#define MUSICXML_H

#include <string>
#include <vector>

// converts a MusicXML file (.xml or .musicxml) or a compressed MusicXML file (.mxl) to LiveLily code
// it only reads the file and returns the lines of LiveLily code, which the editor then inserts (see Editor::loadFile())
class MusicXMLLoader
{
	public:
		// tab is the string used for indentation (the instrument lines inside bars)
		MusicXMLLoader(const std::string& tab);
		// returns the lines of LiveLily code, including the empty lines between bars
		// if the file can't be read or converted, it returns no lines, and getError() returns the reason
		std::vector<std::string> load(const std::string& filePath);
		std::string getError();
	private:
		void convert(const std::string& xml);
		// compressed MusicXML (.mxl) is a zip archive, with the score in one of its files
		// the functions below find that file and return its contents, or an empty string and an error
		bool isZipArchive(const std::string& data);
		std::string extractFromZip(const std::string& data, const std::string& entryName);
		std::vector<std::string> listZipEntries(const std::string& data);
		std::string musicXMLFromMxl(const std::string& data);
		std::string error;
		// rewrites XML with every element on its own line, as the converter reads the file line by line
		static std::string oneElementPerLine(const std::string& xml);
		void createNewLine(const std::string& str, int increment);
		// safe parsing of MusicXML values, so that malformed files don't crash the loader
		int xmlInt(const std::string& str, int fallback);
		std::string xmlAttribute(const std::string& line, const std::string& name);
		bool isXmlElement(const std::string& line, const std::string& name);
		// the text of a one-line element, without its tags and attributes
		std::string xmlElementText(const std::string& line);
		std::string xmlText(const std::string& str);
		std::string orderNoteMarks(const std::string& marks);
		int xmlMeasureNumber(const std::string& line, int prevNumber);
		size_t findChordEnd(std::string str);
		size_t findChordStart(std::string str);
		std::vector<std::string> lines;
		int cursorLineIndex;
		std::string tabStr;
};

#endif
