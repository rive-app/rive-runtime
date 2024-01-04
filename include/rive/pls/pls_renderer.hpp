/*
 * Copyright 2022 Rive
 */

#pragma once

#include "rive/math/raw_path.hpp"
#include "rive/renderer.hpp"
#include "rive/pls/pls.hpp"
#include "rive/pls/pls_render_context.hpp"
#include <vector>

namespace rive
{
class GrInnerFanTriangulator;
};

namespace rive::pls
{
class PLSDraw;
class PLSPath;
class PLSPaint;
class PLSRenderContext;

// Renderer implementation for Rive's pixel local storage renderer.
class PLSRenderer : public Renderer
{
public:
    PLSRenderer(PLSRenderContext*);
    ~PLSRenderer() override;

    void save() override;
    void restore() override;
    void transform(const Mat2D& matrix) override;
    void drawPath(RenderPath*, RenderPaint*) override;
    void clipPath(RenderPath*) override;
    void drawImage(const RenderImage*, BlendMode, float opacity) override;
    void drawImageMesh(const RenderImage*,
                       rcp<RenderBuffer> vertices_f32,
                       rcp<RenderBuffer> uvCoords_f32,
                       rcp<RenderBuffer> indices_u16,
                       uint32_t vertexCount,
                       uint32_t indexCount,
                       BlendMode,
                       float opacity) override;

    // Determines if a path is an axis-aligned rectangle that can be represented by rive::AABB.
    static bool IsAABB(const RawPath&, AABB* result);

#ifdef TESTING
    bool hasClipRect() const { return m_stack.back().clipRectInverseMatrix != nullptr; }
    const AABB& getClipRect() const { return m_stack.back().clipRect; }
    const Mat2D& getClipRectMatrix() const { return m_stack.back().clipRectMatrix; }
#endif

private:
    void clipRectImpl(AABB, const PLSPath* originalPath);
    void clipPathImpl(const PLSPath*);

    // Clips and pushes the given draw to m_context. If the clipped draw is too complex to be
    // supported by the GPU buffers, even after a clean flush, then nothing is drawn.
    void clipAndPushDraw(PLSDraw*);

    // Pushes any necessary clip updates to m_internalDrawBatch and sets the PLSDraw's clipID and
    // clipRectInverseMatrix, if any.
    // Returns false if the operation failed, at which point the caller should flush and try again.
    [[nodiscard]] bool applyClip(PLSDraw*);

    // Calls releaseRefs() on every draw in m_internalDrawBatch and clears the array.
    void releaseInternalDrawBatch();

    struct RenderState
    {
        Mat2D matrix;
        size_t clipStackHeight = 0;
        AABB clipRect;
        Mat2D clipRectMatrix;
        const pls::ClipRectInverseMatrix* clipRectInverseMatrix = nullptr;
    };
    std::vector<RenderState> m_stack{1};

    struct ClipElement
    {
        ClipElement() = default;
        ClipElement(const Mat2D&, const PLSPath*, FillRule);
        ~ClipElement();

        void reset(const Mat2D&, const PLSPath*, FillRule);
        bool isEquivalent(const Mat2D&, const PLSPath*) const;

        Mat2D matrix;
        uint64_t rawPathMutationID;
        AABB pathBounds;
        rcp<const PLSPath> path;
        FillRule fillRule; // Bc PLSPath fillRule can mutate during the artboard draw process.
        uint32_t clipID;
    };
    std::vector<ClipElement> m_clipStack;
    uint64_t m_clipStackFlushID = -1; // Ensures we invalidate the clip stack after a flush.

    PLSRenderContext* const m_context;

    std::vector<PLSDraw*> m_internalDrawBatch;

    // Path of the rectangle [0, 0, 1, 1]. Used to draw images.
    rcp<PLSPath> m_unitRectPath;

    // Used to build coarse path interiors for the "interior triangulation" algorithm.
    RawPath m_scratchPath;
};
} // namespace rive::pls
