#ifndef OF_CANVAS_H
#define OF_CANVAS_H

#include "canvas.h"

// the canvas of the views with openFrameworks (see canvas.h), which draws in the window of the app
class OfCanvas : public Canvas
{
	public:
		std::shared_ptr<Font> loadFont(const std::string& path, int size) override;
		void setColor(Color color) override;
		void setLineWidth(float width) override;
		void setFill(bool fill) override;
		void line(float x1, float y1, float x2, float y2) override;
		void rect(float x, float y, float width, float height) override;
		void circle(float x, float y, float radius) override;
		void quad(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4) override;
		void beginShape() override;
		void vertex(float x, float y) override;
		void bezierVertex(float cx1, float cy1, float cx2, float cy2, float x, float y) override;
		void endShape(bool close) override;
		void push() override;
		void pop() override;
		void translate(float x, float y) override;
		void scale(float x, float y) override;
};

#endif
