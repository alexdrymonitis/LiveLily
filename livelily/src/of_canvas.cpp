#include "of_canvas.h"
#include "ofMain.h"

namespace {

class OfFont : public Font
{
	public:
		OfFont(const std::string& path, int size) { font.load(path, size); }
		void draw(const std::string& text, float x, float y) override { font.drawString(text, x, y); }
		float width(const std::string& text) override { return font.stringWidth(text); }
		float height(const std::string& text) override { return font.stringHeight(text); }
		Rect bounds(const std::string& text, float x, float y) override
		{
			ofRectangle r = font.getStringBoundingBox(text, x, y);
			return {r.x, r.y, r.width, r.height};
		}
	private:
		ofTrueTypeFont font;
};

}

std::shared_ptr<Font> OfCanvas::loadFont(const std::string& path, int size)
{
	return std::make_shared<OfFont>(path, size);
}

void OfCanvas::setColor(Color color) { ofSetColor(color.r, color.g, color.b, color.a); }
void OfCanvas::setLineWidth(float width) { ofSetLineWidth(width); }
void OfCanvas::setFill(bool fill) { if (fill) ofFill(); else ofNoFill(); }
void OfCanvas::line(float x1, float y1, float x2, float y2) { ofDrawLine(x1, y1, x2, y2); }
void OfCanvas::rect(float x, float y, float width, float height) { ofDrawRectangle(x, y, width, height); }
void OfCanvas::circle(float x, float y, float radius) { ofDrawCircle(x, y, radius); }

void OfCanvas::quad(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4)
{
	ofMesh m;
	m.setMode(OF_PRIMITIVE_TRIANGLE_STRIP);
	m.addVertex(glm::vec3(x1, y1, 0));
	m.addVertex(glm::vec3(x2, y2, 0));
	m.addVertex(glm::vec3(x3, y3, 0));
	m.addVertex(glm::vec3(x4, y4, 0));
	m.draw();
}

void OfCanvas::beginShape() { ofBeginShape(); }
void OfCanvas::vertex(float x, float y) { ofVertex(x, y); }
void OfCanvas::bezierVertex(float cx1, float cy1, float cx2, float cy2, float x, float y) { ofBezierVertex(cx1, cy1, cx2, cy2, x, y); }
void OfCanvas::endShape(bool close) { ofEndShape(close); }
void OfCanvas::push() { ofPushMatrix(); }
void OfCanvas::pop() { ofPopMatrix(); }
void OfCanvas::translate(float x, float y) { ofTranslate(x, y); }
void OfCanvas::scale(float x, float y) { ofScale(x, y); }
