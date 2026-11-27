#include "juce_canvas.h"
#include <cmath>
#include <filesystem>

namespace {

class JuceFont : public Font
{
	public:
		JuceFont(JuceCanvas& canvas, juce::Typeface::Ptr typeface, int size)
			: canvas(canvas), font(juce::FontOptions(typeface).withPointHeight((float)size)) {}

		void draw(const std::string& text, float x, float y) override
		{
			juce::Graphics* g = canvas.graphics();
			if (g == nullptr || text.empty()) return;
			juce::GlyphArrangement glyphs;
			glyphs.addLineOfText(font, juce::String::fromUTF8(text.c_str()), x, y);
			// every glyph at the advances rounded to whole pixels, as openFrameworks drew text, so that the text of the editor stays
			// in its cells, which are as wide as a rounded advance (see width())
			float pen = x;
			for (int i = 0; i < glyphs.getNumGlyphs(); i++) {
				juce::PositionedGlyph& glyph = glyphs.getGlyph(i);
				float advance = std::round(glyph.getRight() - glyph.getLeft());
				glyph.moveBy(pen - glyph.getLeft(), 0);
				pen += advance;
			}
			glyphs.draw(*g);
		}
		// by the advances of the glyphs, each rounded to a whole pixel, as openFrameworks measured them
		float width(const std::string& text) override
		{
			juce::GlyphArrangement glyphs;
			glyphs.addLineOfText(font, juce::String::fromUTF8(text.c_str()), 0, 0);
			float total = 0;
			for (int i = 0; i < glyphs.getNumGlyphs(); i++) total += std::round(glyphs.getGlyph(i).getRight() - glyphs.getGlyph(i).getLeft());
			return total;
		}
		// the height and the bounds of text as openFrameworks measured them (ofTrueTypeFont::getStringBoundingBox()), which the editor's cells
		// and the layout of the score were made with: every glyph spans from the baseline minus the height of its ink
		// down to the lower of the baseline and the bottom of its ink (so a glyph below the baseline counts its descent twice)
		// with the width of the advances
		float height(const std::string& text) override { return std::round(bounds(text, 0, 0).height); }
		Rect bounds(const std::string& text, float x, float y) override
		{
			juce::GlyphArrangement glyphs;
			glyphs.addLineOfText(font, juce::String::fromUTF8(text.c_str()), x, y);
			float minY = y, maxY = y, right = x;
			for (int i = 0; i < glyphs.getNumGlyphs(); i++) {
				const juce::PositionedGlyph& glyph = glyphs.getGlyph(i);
				right = std::max(right, x + std::round(glyph.getRight() - x));
				juce::Path path;
				glyph.createPath(path);
				if (path.isEmpty()) continue;
				juce::Rectangle<float> ink = path.getBounds();
				float inkTop = ink.getY() - y, inkBottom = ink.getBottom() - y;
				minY = std::min(minY, y - (inkBottom - inkTop));
				maxY = std::max(maxY, y + inkBottom);
			}
			return {x, minY, right - x, maxY - minY};
		}
		Rect ink(const std::string& text) override
		{
			juce::GlyphArrangement glyphs;
			glyphs.addLineOfText(font, juce::String::fromUTF8(text.c_str()), 0, 0);
			juce::Rectangle<float> box;
			bool any = false;
			for (int i = 0; i < glyphs.getNumGlyphs(); i++) {
				juce::Path path;
				glyphs.getGlyph(i).createPath(path);
				if (path.isEmpty()) continue;
				box = (any ? box.getUnion(path.getBounds()) : path.getBounds());
				any = true;
			}
			return {box.getX(), box.getY(), box.getWidth(), box.getHeight()};
		}
		float lineHeight() override { return font.getHeight(); }

	private:
		JuceCanvas& canvas;
		juce::Font font;
};

}

juce::Typeface::Ptr JuceCanvas::typeface(const std::string& path)
{
	auto it = typefaces.find(path);
	if (it != typefaces.end()) return it->second;
	juce::File file(juce::String((std::filesystem::path(resources) / path).string()));
	juce::MemoryBlock data;
	juce::Typeface::Ptr face;
	if (file.loadFileAsData(data)) face = juce::Typeface::createSystemTypefaceFor(data.getData(), data.getSize());
	typefaces[path] = face;
	return face;
}

std::shared_ptr<Font> JuceCanvas::loadFont(const std::string& path, int size)
{
	juce::Typeface::Ptr face = typeface(path);
	if (face == nullptr || size <= 0) return noFont();
	return std::make_shared<JuceFont>(*this, face, size);
}

void JuceCanvas::setColor(Color color)
{
	if (g) g->setColour(juce::Colour(color.r, color.g, color.b, color.a));
}

void JuceCanvas::line(float x1, float y1, float x2, float y2)
{
	if (g) g->drawLine(x1, y1, x2, y2, lineWidthNow);
}

void JuceCanvas::rect(float x, float y, float width, float height)
{
	if (!g) return;
	juce::Rectangle<float> r(x, y, width, height);
	if (fill) g->fillRect(r);
	else g->drawRect(r, lineWidthNow);
}

void JuceCanvas::circle(float x, float y, float radius)
{
	if (!g) return;
	if (fill) g->fillEllipse(x - radius, y - radius, radius * 2, radius * 2);
	else g->drawEllipse(x - radius, y - radius, radius * 2, radius * 2, lineWidthNow);
}

void JuceCanvas::quad(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4)
{
	if (!g) return;
	// the corners are those of a strip of two triangles (the first three, and the last three), filled as one shape, around its outline,
	// as two triangles filled with antialiasing leave a seam where they meet (a light line across a beam)
	juce::Path p;
	p.startNewSubPath(x1, y1);
	p.lineTo(x2, y2);
	p.lineTo(x4, y4);
	p.lineTo(x3, y3);
	p.closeSubPath();
	g->fillPath(p);
}

void JuceCanvas::beginShape()
{
	shape.clear();
	shapeStarted = false;
}

void JuceCanvas::vertex(float x, float y)
{
	if (!shapeStarted) {
		shape.startNewSubPath(x, y);
		shapeStarted = true;
	}
	else shape.lineTo(x, y);
}

void JuceCanvas::bezierVertex(float cx1, float cy1, float cx2, float cy2, float x, float y)
{
	if (!shapeStarted) {
		shape.startNewSubPath(x, y);
		shapeStarted = true;
		return;
	}
	shape.cubicTo(cx1, cy1, cx2, cy2, x, y);
}

void JuceCanvas::endShape(bool close)
{
	if (close) shape.closeSubPath();
	if (g && shapeStarted) {
		if (fill) g->fillPath(shape);
		else g->strokePath(shape, juce::PathStrokeType(lineWidthNow));
	}
	shape.clear();
	shapeStarted = false;
}

void JuceCanvas::polyline(const std::vector<std::pair<float, float>>& points)
{
	if (!g || points.size() < 2) return;
	juce::Path p;
	p.startNewSubPath(points[0].first, points[0].second);
	for (size_t i = 1; i < points.size(); i++) p.lineTo(points[i].first, points[i].second);
	g->strokePath(p, juce::PathStrokeType(lineWidthNow));
}

void JuceCanvas::push()
{
	if (g) g->saveState();
}

void JuceCanvas::pop()
{
	if (g) g->restoreState();
}

void JuceCanvas::translate(float x, float y)
{
	if (g) g->addTransform(juce::AffineTransform::translation(x, y));
}

void JuceCanvas::scale(float x, float y)
{
	if (g) g->addTransform(juce::AffineTransform::scale(x, y));
}
