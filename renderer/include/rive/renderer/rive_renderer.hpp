/*
 * Copyright 2022 Rive
 */

#pragma once

#include "rive/math/raw_path.hpp"
#include "rive/renderer.hpp"
#include "rive/renderer/gpu.hpp"
#include "rive/renderer/draw.hpp"
#include "rive/renderer/render_context.hpp"
#include <vector>

namespace rive
{
class GrInnerFanTriangulator;
};

namespace rive::gpu
{
class RiveRenderPath;
class RiveRenderPaint;
class PLSRenderContext;

// Renderer implementation for Rive's pixel local storage renderer.
class RiveRenderer : public Renderer
{
public:
    RiveRenderer(PLSRenderContext*);
    ~RiveRenderer() override;

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
    void clipRectImpl(AABB, const RiveRenderPath* originalPath);
    void clipPathImpl(const RiveRenderPath*);

    // Clips and pushes the given draw to m_context. If the clipped draw is too complex to be
    // supported by the GPU buffers, even after a logical flush, then nothing is drawn.
    void clipAndPushDraw(PLSDrawUniquePtr);

    // Pushes any necessary clip updates to m_internalDrawBatch and sets the PLSDraw's clipID and
    // clipRectInverseMatrix, if any.
    // Returns false if the operation failed, at which point the caller should issue a logical flush
    // and try again.
    [[nodiscard]] bool applyClip(PLSDraw*);

    struct RenderState
    {
        Mat2D matrix;
        size_t clipStackHeight = 0;
        AABB clipRect;
        Mat2D clipRectMatrix;
        const gpu::ClipRectInverseMatrix* clipRectInverseMatrix = nullptr;
        bool clipIsEmpty = false;
    };
    std::vector<RenderState> m_stack{1};

    struct ClipElement
    {
        ClipElement() = default;
        ClipElement(const Mat2D&, const RiveRenderPath*, FillRule);
        ~ClipElement();

        void reset(const Mat2D&, const RiveRenderPath*, FillRule);
        bool isEquivalent(const Mat2D&, const RiveRenderPath*) const;

        Mat2D matrix;
        uint64_t rawPathMutationID;
        AABB pathBounds;
        rcp<const RiveRenderPath> path;
        FillRule
            fillRule; // Bc RiveRenderPath fillRule can mutate during the artboard draw process.
        uint32_t clipID;
    };
    std::vector<ClipElement> m_clipStack;

    PLSRenderContext* const m_context;

    std::vector<PLSDrawUniquePtr> m_internalDrawBatch;

    // Path of the rectangle [0, 0, 1, 1]. Used to draw images.
    rcp<RiveRenderPath> m_unitRectPath;

    // Used to build coarse path interiors for the "interior triangulation" algorithm.
    RawPath m_scratchPath;
};
} // namespace rive::gpu
