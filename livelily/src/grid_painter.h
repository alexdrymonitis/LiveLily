#ifndef GRID_PAINTER_H
#define GRID_PAINTER_H

// paints grids of cells (see cellgrid.h) with the canvas of the front end (see canvas.h) and the font of the editor
// for the front ends that draw (the desktop app)

#include "canvas.h"
#include "cellgrid.h"
#include <vector>

// (in a namespace of its own, as the openFrameworks app also has the painter of openFrameworks, of the same names)
namespace CanvasGrid {

// brightness is the coefficient every color is multiplied with
// the grids an editor has filled, in the order it filled them (see Editor::takePaints())
void paintGrids(const std::vector<GridPaint>& paints, Font& font, float brightness);
void paintGrid(const CellGrid& grid, const GridGeometry& geometry, Font& font, float brightness);
// parts of a grid, for a grid whose parts are placed differently (the status line of a pane, see Editor::drawPaneSeparator())
void paintGridBackgrounds(const CellGrid& grid, const GridGeometry& geometry, float brightness);
void paintGridText(const CellGrid& grid, const GridGeometry& geometry, Font& font, float brightness, int fromCol, int toCol);

}

#endif
