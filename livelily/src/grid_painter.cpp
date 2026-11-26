#include "grid_painter.h"
#include <algorithm>

namespace {

Canvas& cv() { return *Canvas::current; }

void setColor(Color color, float brightness, int alpha = 255)
{
	Color c = atBrightness(color, brightness);
	if (alpha != 255) c.a = (uint8_t)alpha;
	cv().setColor(c);
}

// the baseline of the text of a row, computed in the order the editor computed it, so that it is the same float
float baselineOf(const GridGeometry& g, int row)
{
	return ((row + g.rowOffset + 1) * g.cellHeight) + g.y - g.descent;
}

float rowTop(const GridGeometry& g, int row, bool snap)
{
	float top = ((row + g.rowOffset) * g.cellHeight) + g.y;
	return snap ? (float)(int)top : top;
}

void paintRect(const GridRect& rect, const GridGeometry& g, float brightness)
{
	setColor(rect.color, brightness, rect.alpha);
	if (rect.outline) cv().setFill(false);
	float width = (rect.toEnd ? g.toEndWidth - (rect.col * g.cellWidth) : rect.numCols * g.cellWidth);
	cv().rect(g.x + (rect.col * g.cellWidth), rowTop(g, rect.row, rect.snap), width, rect.numRows * g.cellHeight);
	if (rect.outline) cv().setFill(true);
}

// the text of the cells of a row from a column to another, in runs of the same color, as the editor drew it token by token
void paintText(const CellGrid& grid, const GridGeometry& g, Font& font, float brightness, int row, int fromCol, int toCol)
{
	float baseline = baselineOf(g, row);
	int col = fromCol;
	while (col < toCol) {
		const GridCell& first = grid.at(row, col);
		if (first.ch.empty()) {
			col++;
			continue;
		}
		std::string run;
		int start = col;
		while (col < toCol && !grid.at(row, col).ch.empty() && grid.at(row, col).color == first.color) {
			run += grid.at(row, col).ch;
			col++;
		}
		setColor(first.color, brightness);
		font.draw(run, g.x + g.textInset + (start * g.cellWidth), baseline);
	}
}

void paintCursor(const GridCursor& cursor, const GridGeometry& g, float brightness)
{
	if (cursor.shape == GridCursor::Shape::none) return;
	float x = g.x + (cursor.col * g.cellWidth);
	float y = (g.snapCursor ? (float)(int)rowTop(g, cursor.row, false) : rowTop(g, cursor.row, false) + g.cursorShift);
	setColor(cursor.color, brightness);
	switch (cursor.shape) {
		case GridCursor::Shape::block:
			cv().rect(x, y, g.cellWidth, g.cellHeight);
			break;
		case GridCursor::Shape::outline:
			cv().setFill(false);
			cv().rect(x, y, g.cellWidth, g.cellHeight);
			cv().setFill(true);
			break;
		case GridCursor::Shape::underline:
			cv().line(x, y + g.cellHeight, x + g.cellWidth, y + g.cellHeight);
			break;
		case GridCursor::Shape::bar:
			cv().line(x, y, x, y + g.cellHeight);
			break;
		default:
			break;
	}
}

}

namespace CanvasGrid {

void paintGrid(const CellGrid& grid, const GridGeometry& g, Font& font, float brightness)
{
	for (const GridRect& rect : grid.backgrounds) paintRect(rect, g, brightness);
	for (int row = 0; row < grid.rows; row++) {
		if (grid.cursor.inRow && grid.cursor.row == row) {
			int split = std::clamp(grid.cursor.splitCol, 0, grid.cols);
			paintText(grid, g, font, brightness, row, 0, split);
			paintCursor(grid.cursor, g, brightness);
			paintText(grid, g, font, brightness, row, split, grid.cols);
		}
		else {
			paintText(grid, g, font, brightness, row, 0, grid.cols);
		}
		for (const GridSpan& span : grid.selections) {
			if (span.row != row) continue;
			setColor(span.background, brightness);
			cv().rect(g.x + (span.col * g.cellWidth), rowTop(g, row, span.snap), (int)span.text.size() * g.cellWidth, g.cellHeight);
			setColor(span.textColor, brightness);
			font.draw(span.text, g.x + g.textInset + (span.col * g.cellWidth), baselineOf(g, row));
		}
	}
	for (const GridRect& rect : grid.outlines) paintRect(rect, g, brightness);
	if (!grid.cursor.inRow) paintCursor(grid.cursor, g, brightness);
	for (const GridMark& mark : grid.marks) {
		setColor(mark.cell.color, brightness);
		font.draw(mark.cell.ch, g.x + g.textInset + (mark.col * g.cellWidth), baselineOf(g, mark.row));
	}
	if (!grid.cursor.charOnTop.empty() && grid.cursor.shape != GridCursor::Shape::none) {
		setColor(grid.cursor.charColor, brightness);
		font.draw(grid.cursor.charOnTop, g.x + g.textInset + (grid.cursor.col * g.cellWidth), baselineOf(g, grid.cursor.row));
	}
}

void paintGridBackgrounds(const CellGrid& grid, const GridGeometry& g, float brightness)
{
	for (const GridRect& rect : grid.backgrounds) paintRect(rect, g, brightness);
}

void paintGridText(const CellGrid& grid, const GridGeometry& g, Font& font, float brightness, int fromCol, int toCol)
{
	for (int row = 0; row < grid.rows; row++) paintText(grid, g, font, brightness, row, std::max(0, fromCol), std::min(grid.cols, toCol));
}

void paintGrids(const std::vector<GridPaint>& paints, Font& font, float brightness)
{
	for (const GridPaint& paint : paints) {
		if (!paint.grid) continue;
		switch (paint.part) {
			case GridPaint::Part::backgrounds:
				paintGridBackgrounds(*paint.grid, paint.geometry, brightness);
				break;
			case GridPaint::Part::text:
				paintGridText(*paint.grid, paint.geometry, font, brightness, paint.fromCol, paint.toCol);
				break;
			default:
				paintGrid(*paint.grid, paint.geometry, font, brightness);
				break;
		}
	}
}

}
