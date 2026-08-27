/*
 * Copyright 2026 Rive
 */

#ifndef _RIVE_SVG_RENDERER_HPP_
#define _RIVE_SVG_RENDERER_HPP_

#include "rive/math/mat2d.hpp"
#include "rive/renderer.hpp"
#include "utils/svg_factory.hpp"
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace rive
{

class SVGRenderer : public Renderer
{
public:
    static constexpr int kDefaultFloatPrecision = 4;

    explicit SVGRenderer(int floatPrecision = kDefaultFloatPrecision) :
        m_floatPrecision(floatPrecision)
    {
        m_stack.emplace_back();
        m_defs << std::setprecision(floatPrecision);
    }

    void save() override;
    void restore() override;
    void transform(const Mat2D& transform) override;
    void drawPath(RenderPath* path, RenderPaint* paint) override;
    void clipPath(RenderPath* path) override;
    void drawImage(const RenderImage* image,
                   ImageSampler sampler,
                   BlendMode blendMode,
                   float opacity) override;
    void drawImageMesh(const RenderImage* image,
                       ImageSampler sampler,
                       rcp<RenderBuffer> vertices,
                       rcp<RenderBuffer> uvCoords,
                       rcp<RenderBuffer> indices,
                       uint32_t vertexCount,
                       uint32_t indexCount,
                       BlendMode blendMode,
                       float opacity) override;
    void modulateOpacity(float opacity) override;

    std::string finalize(int width, int height) const;

private:
    // A child of a save block. Either a real draw (a single self-closing
    // <path>/<image> element whose blend is *not* yet inlined as a style),
    // or a pass-through chunk of markup such as a clip-path open/close that
    // should be flushed verbatim and never wrapped by blend grouping.
    struct Item
    {
        std::string body; // the element markup (without blend style for draws)
        BlendMode blend = BlendMode::srcOver;
        bool isDraw = false;
        // clipPath() open items: id of the clip this <g> references.
        int clipOpenId = -1;
        // Restored chunks that are exactly one top-level <g clip-path>: the
        // clip id and the byte length of the opening tag, so writeEntry can
        // merge adjacent chunks sharing a clip into a single group.
        int singleClipId = -1;
        size_t clipOpenLen = 0;
    };

    struct StackEntry
    {
        // Accumulated state. Inherited from the parent on save().
        Mat2D matrix;
        float opacity = 1.0f;

        // Open <g clip-path> wrappers that need closing on restore().
        int openClipGroups = 0;

        std::vector<Item> items;
    };

    int m_floatPrecision;
    std::vector<StackEntry> m_stack;
    std::ostringstream m_defs;
    // Serialized clip geometry -> clip id, so identical clips share one def.
    std::unordered_map<std::string, int> m_clipDefIds;
    int m_clipIdCounter = 0;
    int m_gradientIdCounter = 0;

    void emitPaintAttributes(std::ostream& out,
                             SVGRenderPaint* paint,
                             SVGRenderPath* path,
                             float extraOpacity);

    static void writeTransform(std::ostream& out, const Mat2D& m);
    static void writeColor(std::ostream& out,
                           unsigned r,
                           unsigned g,
                           unsigned b);

    // Append an entry's items into `dst`, applying blend grouping rules and
    // merging adjacent single-clip chunks.
    static void writeEntry(std::ostream& dst, const StackEntry& entry);

    // True when writeEntry will wrap the entry in a shared-blend <g>.
    static bool wantsBlendWrap(const StackEntry& entry);
};

} // namespace rive

#endif
