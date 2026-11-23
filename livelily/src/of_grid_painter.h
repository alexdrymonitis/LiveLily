#ifndef OF_GRID_PAINTER_H
#define OF_GRID_PAINTER_H

// paints a grid of cells (see cellgrid.h) with openFrameworks, the way the editor drew it directly before (see Editor::drawText())

#include "ofMain.h"
#include "cellgrid.h"


// brightness is the coefficient every color is multiplied with (ofApp::brightnessCoeff)
// the grids an editor has filled, in the order it filled them (see Editor::takePaints())
void paintGrids(const std::vector<GridPaint>& paints, ofTrueTypeFont& font, float brightness);
void paintGrid(const CellGrid& grid, const GridGeometry& geometry, ofTrueTypeFont& font, float brightness);
// parts of a grid, for a grid whose parts are placed differently (the status line of a pane, see Editor::drawPaneSeparator())
void paintGridBackgrounds(const CellGrid& grid, const GridGeometry& geometry, float brightness);
void paintGridText(const CellGrid& grid, const GridGeometry& geometry, ofTrueTypeFont& font, float brightness, int fromCol, int toCol);

#endif
