#include "cellgrid.h"
#include <cstdio>
#include <map>
#include <ostream>

namespace {

std::string hex(Color c)
{
	char buf[16];
	std::snprintf(buf, sizeof(buf), "#%02x%02x%02x", c.r, c.g, c.b);
	return buf;
}

// the colors of a grid get letters in the order they first appear, listed with their values
class Palette
{
	public:
		char letter(Color c)
		{
			std::string key = hex(c);
			auto it = letters.find(key);
			if (it != letters.end()) return it->second;
			static const std::string available = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
			char l = (order.size() < available.size() ? available[order.size()] : '?');
			letters[key] = l;
			order.push_back(key);
			return l;
		}
		std::string legend() const
		{
			std::string s;
			for (const std::string& key : order) s += (s.empty() ? "" : " ") + std::string(1, letters.at(key)) + "=" + key;
			return s;
		}
	private:
		std::map<std::string, char> letters;
		std::vector<std::string> order;
};

std::string rtrim(const std::string& s)
{
	size_t end = s.find_last_not_of(' ');
	return (end == std::string::npos ? std::string() : s.substr(0, end + 1));
}

const char* shapeName(GridCursor::Shape shape)
{
	switch (shape) {
		case GridCursor::Shape::block: return "block";
		case GridCursor::Shape::bar: return "bar";
		case GridCursor::Shape::underline: return "underline";
		case GridCursor::Shape::outline: return "outline";
		default: return "none";
	}
}

}

void dumpGrid(std::ostream& out, const std::string& title, const CellGrid& grid)
{
	Palette palette;
	// the text and its colors, row by row, without the empty rows at the end
	int lastRow = -1;
	for (int row = 0; row < grid.rows; row++) {
		for (int col = 0; col < grid.cols; col++) {
			if (!grid.at(row, col).ch.empty()) lastRow = row;
		}
	}
	std::vector<std::string> textRows, colorRows;
	for (int row = 0; row <= lastRow; row++) {
		std::string text, colors;
		for (int col = 0; col < grid.cols; col++) {
			const GridCell& cell = grid.at(row, col);
			text += (cell.ch.empty() ? std::string(" ") : cell.ch);
			colors += (cell.ch.empty() || cell.ch == " " ? ' ' : palette.letter(cell.color));
		}
		textRows.push_back(rtrim(text));
		colorRows.push_back(rtrim(colors));
	}
	std::vector<std::string> layers;
	auto rect = [&](const char* kind, const GridRect& r) {
		std::string s = std::string(kind) + " row " + std::to_string(r.row) + " col " + std::to_string(r.col) + " rows " + std::to_string(r.numRows)
			+ " cols " + (r.toEnd ? std::string("end") : std::to_string(r.numCols)) + " color " + std::string(1, palette.letter(r.color));
		if (r.alpha != 255) s += " alpha " + std::to_string(r.alpha);
		layers.push_back(s);
	};
	for (const GridRect& r : grid.backgrounds) rect("background", r);
	for (const GridSpan& span : grid.selections) {
		layers.push_back("selection row " + std::to_string(span.row) + " col " + std::to_string(span.col) + " \"" + span.text + "\" on "
				+ std::string(1, palette.letter(span.background)) + " in " + std::string(1, palette.letter(span.textColor)));
	}
	for (const GridRect& r : grid.outlines) rect("outline", r);
	for (const GridMark& mark : grid.marks) {
		layers.push_back("mark row " + std::to_string(mark.row) + " col " + std::to_string(mark.col) + " \"" + mark.cell.ch + "\" in "
				+ std::string(1, palette.letter(mark.cell.color)));
	}
	if (grid.cursor.shape != GridCursor::Shape::none) {
		std::string s = std::string("cursor ") + shapeName(grid.cursor.shape) + " row " + std::to_string(grid.cursor.row) + " col " + std::to_string(grid.cursor.col)
			+ " color " + std::string(1, palette.letter(grid.cursor.color));
		if (grid.cursor.inRow) s += " in row before col " + std::to_string(grid.cursor.splitCol);
		if (!grid.cursor.charOnTop.empty()) s += " with \"" + grid.cursor.charOnTop + "\" in " + std::string(1, palette.letter(grid.cursor.charColor));
		layers.push_back(s);
	}
	out << "--- " << title << " (" << grid.rows << " rows, " << grid.cols << " columns)\n";
	out << "colors: " << palette.legend() << "\n";
	for (const std::string& row : textRows) out << "|" << row << "\n";
	out << "colors of the text:\n";
	for (const std::string& row : colorRows) out << "|" << row << "\n";
	for (const std::string& layer : layers) out << layer << "\n";
}
