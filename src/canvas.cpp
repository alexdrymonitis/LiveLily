#include "canvas.h"

// the canvas the views draw with (see canvas.h), which the front end sets when it starts
Canvas* Canvas::current = nullptr;

namespace {
class NoFont : public Font
{
	public:
		void draw(const std::string&, float, float) override {}
		float width(const std::string&) override { return 0; }
		float height(const std::string&) override { return 0; }
		Rect bounds(const std::string&, float x, float y) override { return {x, y, 0, 0}; }
		float lineHeight() override { return 0; }
};
}

std::shared_ptr<Font> noFont()
{
	static std::shared_ptr<Font> font = std::make_shared<NoFont>();
	return font;
}
