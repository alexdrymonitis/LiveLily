#ifndef CELLGRID_H
#define CELLGRID_H

// what the editor shows, as a grid of character cells, which a front end paints (with openFrameworks for now, see of_grid_painter.h)
// the editor fills a grid for every frame, in the layers it draws in (see Editor::drawText()), so that painting them in order
// gives the same picture as drawing directly, while a terminal can paint the same grid in its own cells

#include "color.h"
#include <iosfwd>
#include <string>
#include <vector>

struct GridCell
{
	// one character (UTF-8), or empty for a cell with nothing in it
	std::string ch;
	Color color{};
};

// a rectangle of cells, filled or outlined, with an alpha (255 is opaque)
// toEnd makes it reach the end of its region, which in the desktop app is the edge of the pane, not a column
struct GridRect
{
	int row = 0;
	int col = 0;
	int numRows = 1;
	int numCols = 1;
	bool toEnd = false;
	Color color{};
	int alpha = 255;
	bool outline = false;
	// the top is rounded down to a whole pixel in the desktop app, as the editor did for some rectangles (see GridGeometry)
	bool snap = false;
};

// text of a row drawn on a background of its own, after the text of the row (the selection)
// it can reach past the last column, as the editor drew it
struct GridSpan
{
	int row = 0;
	int col = 0;
	std::string text;
	Color background{};
	Color textColor{};
	bool snap = false;
};

struct GridCursor
{
	enum class Shape { none, block, bar, underline, outline };
	Shape shape = Shape::none;
	int row = 0;
	int col = 0;
	Color color{};
	// the cursor drawn while the text of its row is drawn, before the cells from splitCol on (the token it is on, which is drawn over it)
	// or after the text of all rows (inRow is false)
	bool inRow = false;
	int splitCol = 0;
	// the character drawn on top of a block cursor, in charColor, or empty
	std::string charOnTop;
	Color charColor{};
};

// a character drawn after the cursor (the hint of the arguments of a Python callable)
struct GridMark
{
	int row = 0;
	int col = 0;
	GridCell cell;
};

class CellGrid
{
	public:
		void resize(int numRows, int numCols)
		{
			rows = numRows;
			cols = numCols;
			cells.assign((size_t)(rows * cols), GridCell());
			backgrounds.clear();
			selections.clear();
			outlines.clear();
			marks.clear();
			cursor = GridCursor();
		}
		bool contains(int row, int col) const { return row >= 0 && row < rows && col >= 0 && col < cols; }
		GridCell& at(int row, int col) { return cells[(size_t)(row * cols + col)]; }
		const GridCell& at(int row, int col) const { return cells[(size_t)(row * cols + col)]; }
		// writes a string (one byte per cell, as the editor's text is) from a cell on, cutting it at the end of the row
		void write(int row, int col, const std::string& str, Color color)
		{
			for (size_t i = 0; i < str.size(); i++) {
				if (!contains(row, col + (int)i)) continue;
				GridCell& cell = at(row, col + (int)i);
				cell.ch = std::string(1, str[i]);
				cell.color = color;
			}
		}

		int rows = 0;
		int cols = 0;
		std::vector<GridCell> cells;
		// the layers, in the order they are painted:
		// backgrounds (in their order), then row by row the text and the selections of the row (with the cursor if it is inRow)
		// then the outlines, the cursor (if it is not inRow), the marks, and the character on top of the cursor
		std::vector<GridRect> backgrounds;
		std::vector<GridSpan> selections;
		std::vector<GridRect> outlines;
		std::vector<GridMark> marks;
		GridCursor cursor;
};

// where a grid is in the window, in pixels
// the editor rounded the top of some rectangles down to a whole pixel (the cursor, and the rectangles and selections marked snap)
// which the painter does too, so that the picture stays the same when the height of a row isn't a whole number of pixels
struct GridGeometry
{
	// the left edge of the first column, and the top of the first row
	float x = 0;
	float y = 0;
	int cellWidth = 1;
	float cellHeight = 1;
	// from the baseline of the text to the bottom of its row
	float descent = 0;
	// the width of a rectangle that reaches the end of the region (see GridRect::toEnd)
	float toEndWidth = 0;
	// how far the text is from the left edge of its columns (the padding of the completion pop-up)
	float textInset = 0;
	// the row of the region the first row of the grid is in (the status line of a pane is below its text), so that
	// the tops and baselines of the rows are computed from y as the editor computed them
	int rowOffset = 0;
	// the editor rounds the top of the cursor down to a whole pixel, and the shell draws its cursor lower than its row, by cursorShift
	bool snapCursor = true;
	float cursorShift = 0;
};

// a grid to paint where its geometry says: all of it, its backgrounds only, or the text of some of its columns
// (the status line of a pane has its parts in different places, see Editor::drawPaneSeparator())
struct GridPaint
{
	enum class Part { all, backgrounds, text };
	const CellGrid* grid = nullptr;
	GridGeometry geometry;
	Part part = Part::all;
	int fromCol = 0;
	int toCol = 0;
};

// writes a grid as text, for the test dumps (see testmode.cpp): its text and the colors of its text as letters, row by row
// (with a legend of the colors), then its layers (backgrounds, selections, outlines, marks, and the cursor)
void dumpGrid(std::ostream& out, const std::string& title, const CellGrid& grid);

#endif
