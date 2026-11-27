#ifndef APP_WINDOW_H
#define APP_WINDOW_H

#include "cellgrid.h"
#include <string>
#include <utility>
#include <vector>

// what LiveLily (App, see app.h) asks of the window of its front end: the font of the editor, painting the grids of the editor
// the clipboard, file dialogs, the mouse, the frame rate, full screen, and quitting
// the views draw with the canvas the front end sets (see canvas.h)
class AppWindow
{
	public:
		virtual ~AppWindow() = default;
		// loads the font of the editor at a size (fonts/DroidSansMono.ttf), which the grids are painted with, and measures it:
		// the width of a character, the height of a row, and from the baseline to the bottom of a row (centering the letters in the rows)
		struct FontMetrics
		{
			float cellWidth;
			float cellHeight;
			float characterOffset;
		};
		virtual FontMetrics loadEditorFont(int size) = 0;
		// paints grids of cells (see cellgrid.h) with that font, every color at a brightness from 0 to 1
		// the grids an editor has filled, in their order (see Editor::takePaints()), or one grid
		virtual void paintGrids(const std::vector<GridPaint>& paints, float brightness) = 0;
		virtual void paintGrid(const CellGrid& grid, const GridGeometry& geometry, float brightness) = 0;
		virtual std::string clipboard() = 0;
		virtual void setClipboard(const std::string& text) = 0;
		// the path of a file to save or to open, which the user chooses, or empty
		virtual std::string chooseFileToSave(const std::string& defaultName, const std::string& message) = 0;
		virtual std::string chooseFileToOpen(const std::string& message) = 0;
		virtual void showMouseCursor(bool show) = 0;
		virtual std::pair<int, int> mousePosition() = 0;
		// how many times a second the window is drawn
		virtual float frameRate() = 0;
		virtual void setFrameRate(int framesPerSecond) = 0;
		virtual void toggleFullscreen() = 0;
		// closes the window, once LiveLily has stopped (see App::exit())
		virtual void quit() = 0;
};

#endif
