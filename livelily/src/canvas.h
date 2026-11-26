#ifndef CANVAS_H
#define CANVAS_H

#include "color.h"
#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// what the views of LiveLily (the score, the piano roll, the sequencer, the oscilloscope) draw with, so that they are the same
// in every front end that draws them; the front end implements it (with openFrameworks for now, see of_canvas.cpp)
// coordinates are in the units of the window (pixels), with y going down, and text is drawn on its baseline
// colors with alpha blend with what is drawn under them

// a rectangle, as text's bounds are given
struct Rect
{
	float x = 0;
	float y = 0;
	float width = 0;
	float height = 0;
	float centerX() const { return x + (width / 2); }
	float centerY() const { return y + (height / 2); }
};

// a font of a size, which draws text in the color of the canvas, and measures it
class Font
{
	public:
		virtual ~Font() = default;
		// draws text with its baseline at y
		virtual void draw(const std::string& text, float x, float y) = 0;
		virtual float width(const std::string& text) = 0;
		virtual float height(const std::string& text) = 0;
		// the bounds of the text, drawn with its baseline at y
		virtual Rect bounds(const std::string& text, float x, float y) = 0;
		// the distance between the baselines of two lines of text
		virtual float lineHeight() = 0;
};

// a font that draws nothing and measures nothing, for a font that hasn't been loaded yet
std::shared_ptr<Font> noFont();

class Canvas
{
	public:
		virtual ~Canvas() = default;
		// the canvas the views draw with, which the front end sets when it starts
		static Canvas* current;

		// a font of the resources of LiveLily (e.g. fonts/Bravura.otf); one that can't be loaded draws nothing and measures nothing
		virtual std::shared_ptr<Font> loadFont(const std::string& path, int size) = 0;
		// a music font with the code points of SMuFL (fonts/Bravura.otf, see smufl.h), whose glyphs are in the Private Use Area of Unicode
		virtual std::shared_ptr<Font> loadMusicFont(const std::string& path, int size) = 0;

		virtual void setColor(Color color) = 0;
		virtual void setLineWidth(float width) = 0;
		// whether shapes (rectangles, circles, and those of beginShape()) are filled, or only their outline is drawn
		virtual void setFill(bool fill) = 0;
		virtual void line(float x1, float y1, float x2, float y2) = 0;
		virtual void rect(float x, float y, float width, float height) = 0;
		virtual void circle(float x, float y, float radius) = 0;
		// a filled quadrilateral, as two triangles: the first three corners, and the last three (a triangle strip)
		virtual void quad(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4) = 0;
		// a shape of straight lines and cubic Bézier curves, from the first vertex on, closed or open
		virtual void beginShape() = 0;
		virtual void vertex(float x, float y) = 0;
		virtual void bezierVertex(float cx1, float cy1, float cx2, float cy2, float x, float y) = 0;
		virtual void endShape(bool close) = 0;
		// lines through points in order (e.g. a waveform)
		virtual void polyline(const std::vector<std::pair<float, float>>& points) = 0;
		// transformations, for what follows until pop()
		virtual void push() = 0;
		virtual void pop() = 0;
		virtual void translate(float x, float y) = 0;
		virtual void scale(float x, float y) = 0;

		// what the front end sets for the views: the brightness the colors are drawn at, from 0 to 1, and the width of the lines of the panes
		float brightness = 1;
		float lineWidth = 2;

		// a line as thick as width on either side, as a filled quadrilateral (e.g. a beam)
		void thickLine(float x1, float y1, float x2, float y2, float width)
		{
			float dx = x1 - x2, dy = y1 - y2;
			float length = std::sqrt((dx * dx) + (dy * dy));
			if (length == 0) return;
			// the direction across the line
			float nx = -(dy / length) * width, ny = (dx / length) * width;
			quad(x1 + nx, y1 + ny, x1 - nx, y1 - ny, x2 + nx, y2 + ny, x2 - nx, y2 - ny);
		}
};

// a canvas that draws nothing, for a front end that lays the views out without drawing them (the headless runner)
// whose fonts draw and measure nothing
class NullCanvas : public Canvas
{
	public:
		std::shared_ptr<Font> loadFont(const std::string&, int) override { return noFont(); }
		std::shared_ptr<Font> loadMusicFont(const std::string&, int) override { return noFont(); }
		void setColor(Color) override {}
		void setLineWidth(float) override {}
		void setFill(bool) override {}
		void line(float, float, float, float) override {}
		void rect(float, float, float, float) override {}
		void circle(float, float, float) override {}
		void quad(float, float, float, float, float, float, float, float) override {}
		void beginShape() override {}
		void vertex(float, float) override {}
		void bezierVertex(float, float, float, float, float, float) override {}
		void endShape(bool) override {}
		void polyline(const std::vector<std::pair<float, float>>&) override {}
		void push() override {}
		void pop() override {}
		void translate(float, float) override {}
		void scale(float, float) override {}
};

// a color at the brightness of the canvas, every channel truncated to a whole value, as openFrameworks multiplies colors
// keeping its alpha, or opaque
inline Color atBrightness(Color color, float brightness)
{
	return Color((uint8_t)(color.r * brightness), (uint8_t)(color.g * brightness), (uint8_t)(color.b * brightness), color.a);
}
// a color from numbers, each truncated to a whole value from 0 to 255, as openFrameworks sets colors from numbers
inline Color rgba(float r, float g, float b, float a = 255)
{
	auto channel = [](float v) { int i = (int)v; return (uint8_t)(i < 0 ? 0 : (i > 255 ? 255 : i)); };
	return Color(channel(r), channel(g), channel(b), channel(a));
}
inline Color opaqueAtBrightness(Color color, float brightness)
{
	Color c = atBrightness(color, brightness);
	c.a = 255;
	return c;
}

#endif
