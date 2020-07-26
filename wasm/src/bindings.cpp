#include "artboard.hpp"
#include "core/binary_reader.hpp"
#include "file.hpp"
#include "math/mat2d.hpp"
#include "renderer.hpp"
#include <emscripten.h>
#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <stdio.h>
#include <string>
#include <vector>

using namespace emscripten;

class RendererWrapper : public wrapper<rive::Renderer>
{
public:
	EMSCRIPTEN_WRAPPER(RendererWrapper);

	void save() override { call<void>("save"); }

	void restore() override { call<void>("restore"); }

	void transform(const rive::Mat2D& transform) override
	{
		call<void>("transform", transform);
	}

	void translate(float x, float y) override { call<void>("translate", x, y); }

	void drawPath(rive::RenderPath* path, rive::RenderPaint* paint) override
	{
		call<void>("drawPath", path, paint);
	}

	void clipPath(rive::RenderPath* path) override
	{
		call<void>("clipPath", path);
	}
};

class RenderPathWrapper : public wrapper<rive::RenderPath>
{
public:
	EMSCRIPTEN_WRAPPER(RenderPathWrapper);

	void reset() override { call<void>("reset"); }
	// TODO: add commands like cubicTo, moveTo, etc...
	void addPath(rive::RenderPath* path, const rive::Mat2D& transform) override
	{
		call<void>("addPath", path, transform);
	}

	void moveTo(float x, float y) override { call<void>("moveTo", x, y); }
	void lineTo(float x, float y) override { call<void>("lineTo", x, y); }
	void
	cubicTo(float ox, float oy, float ix, float iy, float x, float y) override
	{
		call<void>("cubicTo", ox, oy, ix, iy, x, y);
	}
	void close() override { call<void>("close"); }
};

class RenderPaintWrapper : public wrapper<rive::RenderPaint>
{
public:
	EMSCRIPTEN_WRAPPER(RenderPaintWrapper);

	void color(unsigned int value) { call<void>("color", value); }
	void style(rive::RenderPaintStyle value) { call<void>("style", value); }
};

namespace rive
{
	RenderPaint* makeRenderPaint()
	{
		val renderPaint =
		    val::module_property("renderFactory").call<val>("makeRenderPaint");
		return renderPaint.as<RenderPaint*>(allow_raw_pointers());
	}

	RenderPath* makeRenderPath()
	{
		val renderPath =
		    val::module_property("renderFactory").call<val>("makeRenderPath");
		return renderPath.as<RenderPath*>(allow_raw_pointers());
	}
} // namespace rive

rive::File* load(emscripten::val byteArray)
{
	std::vector<unsigned char> rv;

	const auto l = byteArray["byteLength"].as<unsigned>();
	rv.resize(l);

	emscripten::val memoryView{emscripten::typed_memory_view(l, rv.data())};
	memoryView.call<void>("set", byteArray);
	auto reader = rive::BinaryReader(rv.data(), rv.size());
	rive::File* file = nullptr;
	auto result = rive::File::import(reader, &file);
	return file;
}
EMSCRIPTEN_BINDINGS(RiveWASM)
{
	function("load", &load, allow_raw_pointers());

	class_<rive::Renderer>("Renderer")
	    .function("save",
	              &RendererWrapper::save,
	              pure_virtual(),
	              allow_raw_pointers())
	    .function("restore",
	              &RendererWrapper::restore,
	              pure_virtual(),
	              allow_raw_pointers())
	    .function("transform",
	              &RendererWrapper::transform,
	              pure_virtual(),
	              allow_raw_pointers())
	    .function("translate",
	              &RendererWrapper::translate,
	              pure_virtual(),
	              allow_raw_pointers())
	    .function("drawPath",
	              &RendererWrapper::drawPath,
	              pure_virtual(),
	              allow_raw_pointers())
	    .function("clipPath",
	              &RendererWrapper::clipPath,
	              pure_virtual(),
	              allow_raw_pointers())
	    .allow_subclass<RendererWrapper>("RendererWrapper");

	class_<rive::RenderPath>("RenderPath")
	    .function("reset",
	              &RenderPathWrapper::reset,
	              pure_virtual(),
	              allow_raw_pointers())
	    .function("addPath",
	              &RenderPathWrapper::addPath,
	              pure_virtual(),
	              allow_raw_pointers())
	    .function("moveTo",
	              &RenderPathWrapper::moveTo,
	              pure_virtual(),
	              allow_raw_pointers())
	    .function("lineTo",
	              &RenderPathWrapper::lineTo,
	              pure_virtual(),
	              allow_raw_pointers())
	    .function("cubicTo",
	              &RenderPathWrapper::cubicTo,
	              pure_virtual(),
	              allow_raw_pointers())
	    .function("close",
	              &RenderPathWrapper::close,
	              pure_virtual(),
	              allow_raw_pointers())
	    .allow_subclass<RenderPathWrapper>("RenderPathWrapper");

	enum_<rive::RenderPaintStyle>("RenderPaintStyle")
	    .value("Fill", rive::RenderPaintStyle::Fill)
	    .value("Stroke", rive::RenderPaintStyle::Stroke);

	class_<rive::RenderPaint>("RenderPaint")
	    .function("color",
	              &RenderPaintWrapper::color,
	              pure_virtual(),
	              allow_raw_pointers())
	    .function("style",
	              &RenderPaintWrapper::style,
	              pure_virtual(),
	              allow_raw_pointers())
	    .allow_subclass<RenderPaintWrapper>("RenderPaintWrapper");

	// class_<RendererNoOp>("RendererNoOp")
	//     .constructor<>()
	//     .function("save", &RendererNoOp::save)
	//     .function("transform", &RendererNoOp::transform,
	//     allow_raw_pointers());

	class_<rive::Mat2D>("Mat2D")
	    .property("xx", &rive::Mat2D::xx)
	    .property("xy", &rive::Mat2D::xy)
	    .property("yx", &rive::Mat2D::yx)
	    .property("yy", &rive::Mat2D::yy)
	    .property("tx", &rive::Mat2D::tx)
	    .property("ty", &rive::Mat2D::ty);

	class_<rive::File>("File")
	    .function("artboard",
	              select_overload<rive::Artboard*(std::string) const>(
	                  &rive::File::artboard),
	              allow_raw_pointers())
	    .function(
	        "defaultArtboard",
	        select_overload<rive::Artboard*() const>(&rive::File::artboard),
	        allow_raw_pointers());

	class_<rive::Artboard>("Artboard")
	    .function("advance", &rive::Artboard::advance)
	    .function("draw", &rive::Artboard::draw, allow_raw_pointers());
}