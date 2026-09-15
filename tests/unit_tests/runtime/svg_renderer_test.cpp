// Unit tests for the SVG renderer (SVGRenderer).
//
// These tests exercise SVGRenderer directly via SVGFactory, asserting on the
// exact structure of the SVG produced by finalize(). They verify the
// flat-output / lazy-group behavior described in
// `flatten_svg_extractor_output` plan.

#include "rive/file.hpp"
#include "rive/math/mat2d.hpp"
#include "rive/renderer.hpp"
#include "rive/shapes/paint/color.hpp"
#include "utils/svg_factory.hpp"
#include "utils/svg_renderer.hpp"
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <string>

using rive::SVGFactory;
using rive::SVGRenderer;
using rive::SVGRenderPaint;

namespace
{

// Returns the substring of `out` between the closing of the <svg> opening tag
// (or the closing </defs>, whichever is later) and the closing </svg>.
std::string body(const std::string& out)
{
    auto svgClose = out.rfind("</svg>");
    REQUIRE(svgClose != std::string::npos);
    auto end = svgClose;

    auto defsClose = out.find("</defs>");
    if (defsClose != std::string::npos)
    {
        return out.substr(defsClose + std::string("</defs>").size(),
                          end - (defsClose + std::string("</defs>").size()));
    }
    auto svgOpen = out.find('>');
    REQUIRE(svgOpen != std::string::npos);
    return out.substr(svgOpen + 1, end - (svgOpen + 1));
}

int countOccurrences(const std::string& s, const std::string& needle)
{
    int n = 0;
    size_t pos = 0;
    while ((pos = s.find(needle, pos)) != std::string::npos)
    {
        n++;
        pos += needle.size();
    }
    return n;
}

// Build a simple solid-color fill paint.
rive::rcp<rive::RenderPaint> makeFillPaint(
    SVGFactory& f,
    rive::ColorInt color,
    rive::BlendMode blend = rive::BlendMode::srcOver)
{
    auto p = f.makeRenderPaint();
    static_cast<SVGRenderPaint*>(p.get())->style(rive::RenderPaintStyle::fill);
    static_cast<SVGRenderPaint*>(p.get())->color(color);
    static_cast<SVGRenderPaint*>(p.get())->blendMode(blend);
    return p;
}

rive::rcp<rive::RenderPaint> makeStrokePaint(
    SVGFactory& f,
    rive::ColorInt color,
    float thickness,
    rive::StrokeJoin join = rive::StrokeJoin::miter,
    rive::StrokeCap cap = rive::StrokeCap::butt)
{
    auto p = f.makeRenderPaint();
    auto* sp = static_cast<SVGRenderPaint*>(p.get());
    sp->style(rive::RenderPaintStyle::stroke);
    sp->color(color);
    sp->thickness(thickness);
    sp->join(join);
    sp->cap(cap);
    return p;
}

// A trivial 1-point square path used by most tests.
rive::rcp<rive::RenderPath> makeSquarePath(SVGFactory& f)
{
    rive::RawPath rp;
    rp.moveTo(0, 0);
    rp.line({1, 0});
    rp.line({1, 1});
    rp.line({0, 1});
    rp.close();
    return f.makeRenderPath(rp, rive::FillRule::nonZero);
}

} // namespace

TEST_CASE("SVGRenderer: empty save/restore produces no <g> wrappers",
          "[svg_renderer]")
{
    SVGRenderer r;
    r.save();
    r.restore();
    auto out = r.finalize(10, 10);
    auto b = body(out);
    REQUIRE(countOccurrences(b, "<g") == 0);
    REQUIRE(countOccurrences(b, "</g>") == 0);
}

TEST_CASE(
    "SVGRenderer: pure translation uses translate(x y) and inlines on path",
    "[svg_renderer]")
{
    SVGFactory f;
    SVGRenderer r;
    auto path = makeSquarePath(f);
    auto paint = makeFillPaint(f, 0xFFFF0000);

    r.save();
    r.transform(rive::Mat2D::fromTranslate(10, 20));
    r.drawPath(path.get(), paint.get());
    r.restore();

    auto out = r.finalize(64, 64);
    auto b = body(out);

    REQUIRE(b.find("transform=\"translate(10 20)\"") != std::string::npos);
    REQUIRE(b.find("matrix(") == std::string::npos);
    // No wrapping <g> at all - the transform is on the path itself.
    REQUIRE(countOccurrences(b, "<g") == 0);
    REQUIRE(countOccurrences(b, "<path") == 1);
}

TEST_CASE("SVGRenderer: identity transform is omitted from the path",
          "[svg_renderer]")
{
    SVGFactory f;
    SVGRenderer r;
    auto path = makeSquarePath(f);
    auto paint = makeFillPaint(f, 0xFFFF0000);

    r.save();
    // Identity transform applied explicitly should still produce no
    // transform attribute.
    r.transform(rive::Mat2D());
    r.drawPath(path.get(), paint.get());
    r.restore();

    auto b = body(r.finalize(64, 64));
    REQUIRE(b.find("transform=") == std::string::npos);
    REQUIRE(countOccurrences(b, "<path") == 1);
}

TEST_CASE(
    "SVGRenderer: nested transforms multiply into a single matrix on the path",
    "[svg_renderer]")
{
    SVGFactory f;
    SVGRenderer r;
    auto path = makeSquarePath(f);
    auto paint = makeFillPaint(f, 0xFFFF0000);

    rive::Mat2D A = rive::Mat2D::fromTranslate(10, 20);
    rive::Mat2D B = rive::Mat2D::fromScale(2.0f, 3.0f);
    rive::Mat2D expected = A * B;

    r.save();
    r.transform(A);
    r.save();
    r.transform(B);
    r.drawPath(path.get(), paint.get());
    r.restore();
    r.restore();

    auto b = body(r.finalize(64, 64));
    REQUIRE(countOccurrences(b, "<path") == 1);

    // matrix(2 0 0 3 10 20)
    std::ostringstream want;
    want << "transform=\"matrix(" << expected[0] << ' ' << expected[1] << ' '
         << expected[2] << ' ' << expected[3] << ' ' << expected[4] << ' '
         << expected[5] << ")\"";
    REQUIRE(b.find(want.str()) != std::string::npos);
}

TEST_CASE(
    "SVGRenderer: default-black fill is omitted; non-black uses #rrggbb hex",
    "[svg_renderer]")
{
    SVGFactory f;

    SECTION("Default black, full opacity, no fill attribute")
    {
        SVGRenderer r;
        auto path = makeSquarePath(f);
        auto paint = makeFillPaint(f, 0xFF000000);
        r.drawPath(path.get(), paint.get());
        auto b = body(r.finalize(8, 8));
        REQUIRE(b.find("fill=") == std::string::npos);
    }

    SECTION("Non-black fill emits hex")
    {
        SVGRenderer r;
        auto path = makeSquarePath(f);
        auto paint = makeFillPaint(f, 0xFF12ab34);
        r.drawPath(path.get(), paint.get());
        auto b = body(r.finalize(8, 8));
        REQUIRE(b.find("fill=\"#12ab34\"") != std::string::npos);
        REQUIRE(b.find("rgb(") == std::string::npos);
    }
}

TEST_CASE("SVGRenderer: modulateOpacity multiplies into fill-opacity",
          "[svg_renderer]")
{
    SVGFactory f;

    SECTION("opacity=1.0 produces no fill-opacity attribute")
    {
        SVGRenderer r;
        auto path = makeSquarePath(f);
        auto paint = makeFillPaint(f, 0xFFFF0000);
        r.modulateOpacity(1.0f);
        r.drawPath(path.get(), paint.get());
        auto b = body(r.finalize(8, 8));
        REQUIRE(b.find("fill-opacity") == std::string::npos);
    }

    SECTION("0.5 modulation * paint alpha 0.5 -> fill-opacity 0.25")
    {
        SVGRenderer r;
        auto path = makeSquarePath(f);
        // alpha = 128 / 255 ~= 0.50196
        auto paint = makeFillPaint(f, 0x80FF0000);
        r.save();
        r.modulateOpacity(0.5f);
        r.drawPath(path.get(), paint.get());
        r.restore();
        auto b = body(r.finalize(8, 8));
        // We just verify the attribute is present and starts with 0.25
        auto p = b.find("fill-opacity=\"");
        REQUIRE(p != std::string::npos);
        std::string val =
            b.substr(p + std::string("fill-opacity=\"").size(), 4);
        REQUIRE(val.substr(0, 3) == "0.2");
    }
}

TEST_CASE(
    "SVGRenderer: stroke join/cap defaults are omitted, non-defaults emit",
    "[svg_renderer]")
{
    SVGFactory f;

    SECTION("Default miter + butt: no linejoin/linecap attributes")
    {
        SVGRenderer r;
        auto path = makeSquarePath(f);
        auto paint = makeStrokePaint(f, 0xFF000000, 2.0f);
        r.drawPath(path.get(), paint.get());
        auto b = body(r.finalize(8, 8));
        REQUIRE(b.find("stroke-linejoin") == std::string::npos);
        REQUIRE(b.find("stroke-linecap") == std::string::npos);
    }

    SECTION("Round join + round cap: both attributes present")
    {
        SVGRenderer r;
        auto path = makeSquarePath(f);
        auto paint = makeStrokePaint(f,
                                     0xFF000000,
                                     2.0f,
                                     rive::StrokeJoin::round,
                                     rive::StrokeCap::round);
        r.drawPath(path.get(), paint.get());
        auto b = body(r.finalize(8, 8));
        REQUIRE(b.find("stroke-linejoin=\"round\"") != std::string::npos);
        REQUIRE(b.find("stroke-linecap=\"round\"") != std::string::npos);
    }
}

TEST_CASE(
    "SVGRenderer: clipPath emits exactly one <g clip-path> wrapping its children",
    "[svg_renderer]")
{
    SVGFactory f;
    SVGRenderer r;

    auto clip = makeSquarePath(f);
    auto path = makeSquarePath(f);
    auto paint = makeFillPaint(f, 0xFFFF0000);

    r.save();
    r.clipPath(clip.get());
    r.drawPath(path.get(), paint.get());
    r.drawPath(path.get(), paint.get());
    r.restore();

    auto out = r.finalize(64, 64);
    auto b = body(out);

    REQUIRE(countOccurrences(b, "<g clip-path=\"url(#clip0)\"") == 1);
    REQUIRE(countOccurrences(b, "</g>") == 1);
    REQUIRE(countOccurrences(b, "<path") == 2);
    REQUIRE(out.find("<defs>") != std::string::npos);
    REQUIRE(out.find("<clipPath id=\"clip0\">") != std::string::npos);
}

TEST_CASE(
    "SVGRenderer: clip under a non-identity CTM does not double-transform "
    "children",
    "[svg_renderer]")
{
    SVGFactory f;
    SVGRenderer r;

    auto clip = makeSquarePath(f);
    auto path = makeSquarePath(f);
    auto paint = makeFillPaint(f, 0xFFFF0000);

    // Mirrors a real recording: the extractor aligns (scale 2, offset 10,10),
    // the artboard saves and clips in artboard space, then a shape saves,
    // applies its world transform and draws its local path.
    r.save();
    r.transform(rive::Mat2D(2, 0, 0, 2, 10, 10)); // align
    r.save();
    r.clipPath(clip.get()); // artboard-space clip, CTM = align
    r.save();
    r.transform(rive::Mat2D(1, 0, 0, 1, 5, 7)); // shape world transform
    r.drawPath(path.get(), paint.get());
    r.restore();
    r.restore();
    r.restore();

    auto out = r.finalize(64, 64);
    auto b = body(out);

    // The CTM at clip time is baked into the def's clip geometry...
    REQUIRE(out.find("<clipPath id=\"clip0\">"
                     "<path transform=\"matrix(2 0 0 2 10 10)\"") !=
            std::string::npos);
    // ...and the wrapping <g> carries no transform, so the child's absolute
    // matrix (align * world) is applied exactly once.
    REQUIRE(b.find("<g clip-path=\"url(#clip0)\">") != std::string::npos);
    REQUIRE(countOccurrences(b, "transform=\"matrix(2 0 0 2 20 24)\"") == 1);
}

TEST_CASE("SVGRenderer: identical clip geometry shares one def",
          "[svg_renderer]")
{
    SVGFactory f;
    SVGRenderer r;

    auto clip = makeSquarePath(f);
    auto path = makeSquarePath(f);
    auto paint = makeFillPaint(f, 0xFFFF0000);

    // Two clip scopes with the same geometry, separated by an unclipped
    // draw. Scopes must stay separate (z-order), but share one def.
    r.save();
    r.clipPath(clip.get());
    r.drawPath(path.get(), paint.get());
    r.restore();
    r.drawPath(path.get(), paint.get());
    r.save();
    r.clipPath(clip.get());
    r.drawPath(path.get(), paint.get());
    r.restore();

    auto out = r.finalize(64, 64);
    auto b = body(out);

    REQUIRE(countOccurrences(out, "<clipPath") == 1);
    REQUIRE(countOccurrences(b, "<g clip-path=\"url(#clip0)\">") == 2);
    REQUIRE(countOccurrences(b, "</g>") == 2);
    REQUIRE(countOccurrences(b, "<path") == 3);
}

TEST_CASE("SVGRenderer: same geometry under different CTMs gets distinct defs",
          "[svg_renderer]")
{
    SVGFactory f;
    SVGRenderer r;

    auto clip = makeSquarePath(f);
    auto path = makeSquarePath(f);
    auto paint = makeFillPaint(f, 0xFFFF0000);

    r.save();
    r.transform(rive::Mat2D(2, 0, 0, 2, 0, 0));
    r.clipPath(clip.get());
    r.drawPath(path.get(), paint.get());
    r.restore();
    r.save();
    r.clipPath(clip.get()); // identity CTM: different baked transform
    r.drawPath(path.get(), paint.get());
    r.restore();

    auto out = r.finalize(64, 64);
    REQUIRE(countOccurrences(out, "<clipPath") == 2);
    REQUIRE(out.find("<clipPath id=\"clip0\">") != std::string::npos);
    REQUIRE(out.find("<clipPath id=\"clip1\">") != std::string::npos);
}

TEST_CASE(
    "SVGRenderer: adjacent scopes sharing a clip merge into a single group",
    "[svg_renderer]")
{
    SVGFactory f;
    SVGRenderer r;

    auto clip = makeSquarePath(f);
    auto path = makeSquarePath(f);
    auto paint = makeFillPaint(f, 0xFFFF0000);

    // Three consecutive per-drawable clip scopes with identical geometry and
    // nothing drawn between them — the per-object pattern the runtime emits
    // when a Rive group is clipped. Expect one <g> containing all three.
    for (int i = 0; i < 3; i++)
    {
        r.save();
        r.clipPath(clip.get());
        r.drawPath(path.get(), paint.get());
        r.restore();
    }

    auto out = r.finalize(64, 64);
    auto b = body(out);

    REQUIRE(countOccurrences(out, "<clipPath") == 1);
    REQUIRE(countOccurrences(b, "<g clip-path=\"url(#clip0)\">") == 1);
    REQUIRE(countOccurrences(b, "</g>") == 1);
    REQUIRE(countOccurrences(b, "<path") == 3);
}

TEST_CASE(
    "SVGRenderer: clipped stroke with transformAffectsStroke on keeps its "
    "width, matrix applied once",
    "[svg_renderer]")
{
    SVGFactory f;
    SVGRenderer r;

    auto clip = makeSquarePath(f);
    auto path = makeSquarePath(f);
    auto paint = makeStrokePaint(f, 0xFF00FF00, 4.0f);

    // transformAffectsStroke on = local path: the paint saves and applies the
    // shape's world transform before drawing.
    r.save();
    r.transform(rive::Mat2D(2, 0, 0, 2, 10, 10)); // align
    r.save();
    r.clipPath(clip.get());
    r.save();
    r.transform(rive::Mat2D(3, 0, 0, 3, 5, 7)); // shape world transform
    r.drawPath(path.get(), paint.get());
    r.restore();
    r.restore();
    r.restore();

    auto b = body(r.finalize(64, 64));

    // align * world exactly once; width in local units, scaled by the matrix.
    REQUIRE(countOccurrences(b, "transform=\"matrix(6 0 0 6 20 24)\"") == 1);
    REQUIRE(b.find("stroke-width=\"4\"") != std::string::npos);
    REQUIRE(b.find("<g clip-path=\"url(#clip0)\">") != std::string::npos);
}

TEST_CASE(
    "SVGRenderer: clipped stroke with transformAffectsStroke off draws the "
    "world path in the clip entry",
    "[svg_renderer]")
{
    SVGFactory f;
    SVGRenderer r;

    auto clip = makeSquarePath(f);
    auto path = makeSquarePath(f); // stands in for the pre-transformed
                                   // world-space path
    auto paint = makeStrokePaint(f, 0xFF00FF00, 4.0f);

    // transformAffectsStroke off = world path: no save/transform around the
    // draw, so it lands directly in the clip's save block with CTM = align.
    r.save();
    r.transform(rive::Mat2D(2, 0, 0, 2, 10, 10)); // align
    r.save();
    r.clipPath(clip.get());
    r.drawPath(path.get(), paint.get());
    r.restore();
    r.restore();

    auto out = r.finalize(64, 64);
    auto b = body(out);

    // The stroke path carries only the align matrix (once in the body; the
    // clip def carries its own copy), and sits inside the clip group.
    REQUIRE(countOccurrences(b, "transform=\"matrix(2 0 0 2 10 10)\"") == 1);
    REQUIRE(b.find("stroke-width=\"4\"") != std::string::npos);
    auto groupOpen = b.find("<g clip-path=\"url(#clip0)\">");
    auto strokePath = b.find("stroke-width");
    auto groupClose = b.find("</g>");
    REQUIRE(groupOpen != std::string::npos);
    REQUIRE(strokePath != std::string::npos);
    REQUIRE(groupClose != std::string::npos);
    REQUIRE(groupOpen < strokePath);
    REQUIRE(strokePath < groupClose);
}

TEST_CASE(
    "SVGRenderer: single non-srcOver blend draw inlines style on the path",
    "[svg_renderer]")
{
    SVGFactory f;
    SVGRenderer r;
    auto path = makeSquarePath(f);
    auto paint = makeFillPaint(f, 0xFFFF0000, rive::BlendMode::multiply);

    r.save();
    r.drawPath(path.get(), paint.get());
    r.restore();

    auto b = body(r.finalize(8, 8));
    REQUIRE(b.find("style=\"mix-blend-mode:multiply\"") != std::string::npos);
    REQUIRE(countOccurrences(b, "<g") == 0);
    REQUIRE(countOccurrences(b, "<path") == 1);
}

TEST_CASE(
    "SVGRenderer: multiple shared-blend draws inside one save block wrap in a <g>",
    "[svg_renderer]")
{
    SVGFactory f;
    SVGRenderer r;
    auto path = makeSquarePath(f);
    auto paint = makeFillPaint(f, 0xFFFF0000, rive::BlendMode::multiply);

    r.save();
    r.drawPath(path.get(), paint.get());
    r.drawPath(path.get(), paint.get());
    r.restore();

    auto b = body(r.finalize(8, 8));
    REQUIRE(countOccurrences(b, "<g style=\"mix-blend-mode:multiply\">") == 1);
    REQUIRE(countOccurrences(b, "</g>") == 1);
    REQUIRE(countOccurrences(b, "<path") == 2);
    // Children should NOT have inline blend styles when wrapped in a group.
    REQUIRE(b.find(" d=") != std::string::npos);
    auto pathStart = b.find("<path");
    auto pathEnd = b.find("/>", pathStart);
    REQUIRE(pathEnd != std::string::npos);
    auto firstPath = b.substr(pathStart, pathEnd - pathStart);
    REQUIRE(firstPath.find("mix-blend-mode") == std::string::npos);
}

TEST_CASE(
    "SVGRenderer: identity-only outer save (extractor pattern) produces no wrappers",
    "[svg_renderer]")
{
    SVGFactory f;
    SVGRenderer r;
    auto path = makeSquarePath(f);
    auto paint = makeFillPaint(f, 0xFFFF0000);

    // Mirrors what svg_extractor.cpp does per frame: save, identity align,
    // draw, restore. Should produce a flat single <path>.
    r.save();
    r.transform(rive::Mat2D()); // identity
    r.drawPath(path.get(), paint.get());
    r.restore();

    auto b = body(r.finalize(64, 64));
    REQUIRE(countOccurrences(b, "<g") == 0);
    REQUIRE(countOccurrences(b, "<path") == 1);
}

TEST_CASE(
    "SVGRenderer: svg_clip_test.riv renders a clipped group as one def and one "
    "merged <g>",
    "[svg_renderer]")
{
    // A rectangle clips a group of 4 ellipses. The runtime clips per
    // drawable, so without dedup + adjacent-scope merging this produced 4
    // identical defs and 4 single-ellipse groups.
    SVGFactory factory;
    auto file = ReadRiveFile("assets/svg_clip_test.riv", &factory);
    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);
    artboard->advance(0.0f);

    SVGRenderer r;
    artboard->draw(&r);
    auto out = r.finalize((int)artboard->width(), (int)artboard->height());
    auto b = body(out);

    REQUIRE(countOccurrences(out, "<clipPath") == 1);
    REQUIRE(countOccurrences(b, "<g clip-path=\"url(#clip0)\">") == 1);
    REQUIRE(countOccurrences(b, "</g>") == 1);

    // 4 ellipses inside the clip group, background outside it.
    auto open = b.find("<g clip-path=\"url(#clip0)\">");
    auto close = b.find("</g>");
    REQUIRE(open != std::string::npos);
    REQUIRE(close != std::string::npos);
    REQUIRE(countOccurrences(b.substr(open, close - open), "<path") == 4);
    REQUIRE(countOccurrences(b, "<path") == 5);
}
