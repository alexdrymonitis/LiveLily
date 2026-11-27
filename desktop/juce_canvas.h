#ifndef JUCE_CANVAS_H
#define JUCE_CANVAS_H

#include "canvas.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <map>
#include <memory>
#include <string>
#include <vector>

// the canvas of the views with JUCE (see canvas.h), which draws with the Graphics of the paint it is given (see setGraphics())
// its fonts are files of LiveLily's resources (e.g. fonts/Bravura.otf), measured as openFrameworks measured them:
// widths by the advances of the glyphs, and heights and bounds by the ink of the glyphs
class JuceCanvas : public Canvas
{
	public:
		explicit JuceCanvas(const std::string& resourcesDirectory) : resources(resourcesDirectory) {}
		// the Graphics of the paint that draws, for as long as it draws
		void setGraphics(juce::Graphics* graphics) { g = graphics; }
		juce::Graphics* graphics() const { return g; }
		// a typeface of a font file of the resources, loaded once
		juce::Typeface::Ptr typeface(const std::string& path);

		std::shared_ptr<Font> loadFont(const std::string& path, int size) override;
		std::shared_ptr<Font> loadMusicFont(const std::string& path, int size) override { return loadFont(path, size); }
		void setColor(Color color) override;
		void setLineWidth(float width) override { lineWidthNow = width; }
		void setFill(bool fillShapes) override { fill = fillShapes; }
		void line(float x1, float y1, float x2, float y2) override;
		void rect(float x, float y, float width, float height) override;
		void circle(float x, float y, float radius) override;
		void quad(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4) override;
		void beginShape() override;
		void vertex(float x, float y) override;
		void bezierVertex(float cx1, float cy1, float cx2, float cy2, float x, float y) override;
		void endShape(bool close) override;
		void polyline(const std::vector<std::pair<float, float>>& points) override;
		void push() override;
		void pop() override;
		void translate(float x, float y) override;
		void scale(float x, float y) override;

	private:
		std::string resources;
		juce::Graphics* g = nullptr;
		std::map<std::string, juce::Typeface::Ptr> typefaces;
		float lineWidthNow = 1;
		bool fill = true;
		juce::Path shape;
		bool shapeStarted = false;
};

#endif
