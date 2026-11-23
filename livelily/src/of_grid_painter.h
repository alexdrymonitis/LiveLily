#ifndef OF_GRID_PAINTER_H
#define OF_GRID_PAINTER_H

// paints a grid of cells (see cellgrid.h) with openFrameworks, the way the editor drew it directly before (see Editor::drawText())

#include "ofMain.h"
#include "cellgrid.h"

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

// brightness is the coefficient every color is multiplied with (ofApp::brightnessCoeff)
void paintGrid(const CellGrid& grid, const GridGeometry& geometry, ofTrueTypeFont& font, float brightness);
// parts of a grid, for a grid whose parts are placed differently (the status line of a pane, see Editor::drawPaneSeparator())
void paintGridBackgrounds(const CellGrid& grid, const GridGeometry& geometry, float brightness);
void paintGridText(const CellGrid& grid, const GridGeometry& geometry, ofTrueTypeFont& font, float brightness, int fromCol, int toCol);

#endif
