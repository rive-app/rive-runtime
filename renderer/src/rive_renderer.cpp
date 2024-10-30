/*
 * Copyright 2022 Rive
 */

#include "rive/renderer/rive_renderer.hpp"

#include "rive_render_paint.hpp"
#include "rive_render_path.hpp"
#include "rive/math/math_types.hpp"
#include "rive/math/simd.hpp"
#include "rive/renderer/rive_render_image.hpp"
#include "shaders/constants.glsl"

namespace rive
{
bool RiveRenderer::IsAABB(const RawPath& path, AABB* result)
{
    // Any quadrilateral begins with a move plus 3 lines.
    constexpr static size_t kAABBVerbCount = 4;
    constexpr static PathVerb aabbVerbs[kAABBVerbCount] = {PathVerb::move,
                                                           PathVerb::line,
                                                           PathVerb::line,
                                                           PathVerb::line};
    Span<const PathVerb> verbs = path.verbs();
    if (verbs.count() < kAABBVerbCount ||
        memcmp(verbs.data(), aabbVerbs, sizeof(aabbVerbs)) != 0)
    {
        return false;
    }

    // Only accept extra verbs and points if every point after the quadrilateral
    // is equal to p0.
    Span<const Vec2D> pts = path.points();
    for (size_t i = 4; i < pts.count(); ++i)
    {
        if (pts[i] != pts[0])
        {
            return false;
        }
    }

    // We have a quadrilateral! Now check if it is an axis-aligned rectangle.
    float4 corners = {pts[0].x, pts[0].y, pts[2].x, pts[2].y};
    float4 oppositeCorners = {pts[1].x, pts[1].y, pts[3].x, pts[3].y};
    if (simd::all(corners == oppositeCorners.zyxw) ||
        simd::all(corners == oppositeCorners.xwzy))
    {
        float4 r = simd::join(simd::min(corners.xy, corners.zw),
                              simd::max(corners.xy, corners.zw));
        simd::store(result, r);
        return true;
    }
    return false;
}

RiveRenderer::ClipElement::ClipElement(const Mat2D& matrix_,
                                       const RiveRenderPath* path_,
                                       FillRule fillRule_)
{
    reset(matrix_, path_, fillRule_);
}

RiveRenderer::ClipElement::~ClipElement() {}

void RiveRenderer::ClipElement::reset(const Mat2D& matrix_,
                                      const RiveRenderPath* path_,
                                      FillRule fillRule_)
{
    matrix = matrix_;
    rawPathMutationID = path_->getRawPathMutationID();
    pathBounds = path_->getBounds();
    path = ref_rcp(path_);
    fillRule = fillRule_;
    clipID = 0; // This gets initialized lazily.
}

bool RiveRenderer::ClipElement::isEquivalent(const Mat2D& matrix_,
                                             const RiveRenderPath* path_) const
{
    return matrix_ == matrix &&
           path_->getRawPathMutationID() == rawPathMutationID &&
           path_->getFillRule() == fillRule;
}

RiveRenderer::RiveRenderer(gpu::RenderContext* context) : m_context(context) {}

RiveRenderer::~RiveRenderer() {}

void RiveRenderer::save()
{
    // Copy the back of the stack before pushing, in case the vector grows and
    // invalidates the reference.
    RenderState copy = m_stack.back();
    m_stack.push_back(copy);
}

void RiveRenderer::restore()
{
    assert(m_stack.size() > 1);
    assert(m_stack.back().clipStackHeight >=
           m_stack[m_stack.size() - 2].clipStackHeight);
    m_stack.pop_back();
}

void RiveRenderer::transform(const Mat2D& matrix)
{
    m_stack.back().matrix = m_stack.back().matrix * matrix;
}

void RiveRenderer::drawPath(RenderPath* renderPath, RenderPaint* renderPaint)
{
    LITE_RTTI_CAST_OR_RETURN(path, RiveRenderPath*, renderPath);
    LITE_RTTI_CAST_OR_RETURN(paint, RiveRenderPaint*, renderPaint);

    if (path->getRawPath().empty())
    {
        return;
    }

    bool stroked = paint->getIsStroked();
    if (stroked && m_context->frameDescriptor().strokesDisabled)
    {
        return;
    }
    if (!stroked && m_context->frameDescriptor().fillsDisabled)
    {
        return;
    }
    if (stroked && !(paint->getThickness() >
                     0)) // Use inverse logic to ensure we abort when stroke
    {                    // thickness is NaN.
        return;
    }
    if (m_stack.back().clipIsEmpty)
    {
        return;
    }

    gpu::DrawUniquePtr cacheDraw =
        path->getDrawCache(m_stack.back().matrix,
                           paint,
                           path->getFillRule(),
                           &m_context->perFrameAllocator(),
                           m_context->frameDescriptor(),
                           m_context->frameInterlockMode());

    if (cacheDraw != nullptr)
    {
        clipAndPushDraw(std::move(cacheDraw));
        return;
    }

    auto draw = gpu::RiveRenderPathDraw::Make(m_context,
                                              m_stack.back().matrix,
                                              ref_rcp(path),
                                              path->getFillRule(),
                                              paint,
                                              &m_scratchPath);

    path->setDrawCache(static_cast<gpu::RiveRenderPathDraw*>(draw.get()),
                       m_stack.back().matrix,
                       paint);

    clipAndPushDraw(std::move(draw));
}

void RiveRenderer::clipPath(RenderPath* renderPath)
{
    LITE_RTTI_CAST_OR_RETURN(path, RiveRenderPath*, renderPath);

    if (m_stack.back().clipIsEmpty)
    {
        return;
    }

    if (path->getRawPath().empty())
    {
        m_stack.back().clipIsEmpty = true;
        return;
    }

    // First try to handle axis-aligned rectangles using the "ENABLE_CLIP_RECT"
    // shader feature. Multiple axis-aligned rectangles can be intersected into
    // a single rectangle if their matrices are compatible.
    AABB clipRectCandidate;
    if (m_context->frameSupportsClipRects() &&
        IsAABB(path->getRawPath(), &clipRectCandidate))
    {
        clipRectImpl(clipRectCandidate, path);
    }
    else
    {
        clipPathImpl(path);
    }
}

// Finds a new rect, if such a rect exists, such that:
//
//     currentMatrix * rect == newMatrix * newRect
//
// Returns true if *rect was replaced with newRect.
static bool transform_rect_to_new_space(AABB* rect,
                                        const Mat2D& currentMatrix,
                                        const Mat2D& newMatrix)
{
    if (currentMatrix == newMatrix)
    {
        return true;
    }
    Mat2D currentToNew;
    if (!newMatrix.invert(&currentToNew))
    {
        return false;
    }
    currentToNew = currentToNew * currentMatrix;
    float maxSkew = fmaxf(fabsf(currentToNew.xy()), fabsf(currentToNew.yx()));
    float maxScale = fmaxf(fabsf(currentToNew.xx()), fabsf(currentToNew.yy()));
    if (maxSkew > math::EPSILON && maxScale > math::EPSILON)
    {
        // Transforming this rect to the new view matrix would turn it into
        // something that isn't a rect.
        return false;
    }
    Vec2D pts[2] = {{rect->left(), rect->top()},
                    {rect->right(), rect->bottom()}};
    currentToNew.mapPoints(pts, pts, 2);
    float4 p = simd::load4f(pts);
    float2 topLeft = simd::min(p.xy, p.zw);
    float2 botRight = simd::max(p.xy, p.zw);
    *rect = {topLeft.x, topLeft.y, botRight.x, botRight.y};
    return true;
}

void RiveRenderer::clipRectImpl(AABB rect, const RiveRenderPath* originalPath)
{
    bool hasClipRect = m_stack.back().clipRectInverseMatrix != nullptr;
    if (rect.isEmptyOrNaN())
    {
        m_stack.back().clipIsEmpty = true;
        return;
    }

    // If there already is a clipRect, we can only accept another one by
    // intersecting it with the existing one. This means the new rect must be
    // axis-aligned with the existing clipRect.
    if (hasClipRect &&
        !transform_rect_to_new_space(&rect,
                                     m_stack.back().matrix,
                                     m_stack.back().clipRectMatrix))
    {
        // 'rect' is not axis-aligned with the existing clipRect. Fall back to
        // clipPath.
        clipPathImpl(originalPath);
        return;
    }

    if (!hasClipRect)
    {
        // There wasn't an existing clipRect. This is the one!
        m_stack.back().clipRect = rect;
        m_stack.back().clipRectMatrix = m_stack.back().matrix;
    }
    else
    {
        // Both rects are in the same space now. Intersect the two
        // geometrically.
        float4 a = simd::load4f(&m_stack.back().clipRect);
        float4 b = simd::load4f(&rect);
        float4 intersection =
            simd::join(simd::max(a.xy, b.xy), simd::min(a.zw, b.zw));
        simd::store(&m_stack.back().clipRect, intersection);
    }

    m_stack.back().clipRectInverseMatrix =
        m_context->make<gpu::ClipRectInverseMatrix>(
            m_stack.back().clipRectMatrix,
            m_stack.back().clipRect);
}

void RiveRenderer::clipPathImpl(const RiveRenderPath* path)
{
    if (path->getBounds().isEmptyOrNaN())
    {
        m_stack.back().clipIsEmpty = true;
        return;
    }
    // Only write a new clip element if this path isn't already on the stack
    // from before. e.g.:
    //
    //     clipPath(samePath);
    //     restore();
    //     save();
    //     clipPath(samePath); // <-- reuse the ClipElement (and clipID!)
    //     already in m_clipStack.
    //
    const size_t clipStackHeight = m_stack.back().clipStackHeight;
    assert(m_clipStack.size() >= clipStackHeight);
    if (m_clipStack.size() == clipStackHeight ||
        !m_clipStack[clipStackHeight].isEquivalent(m_stack.back().matrix, path))
    {
        m_clipStack.resize(clipStackHeight);
        m_clipStack.emplace_back(m_stack.back().matrix,
                                 path,
                                 path->getFillRule());
    }
    m_stack.back().clipStackHeight = clipStackHeight + 1;
}

void RiveRenderer::drawImage(const RenderImage* renderImage,
                             BlendMode blendMode,
                             float opacity)
{
    LITE_RTTI_CAST_OR_RETURN(image, const RiveRenderImage*, renderImage);

    // Scale the view matrix so we can draw this image as the rect [0, 0, 1, 1].
    save();
    scale(image->width(), image->height());

    if (!m_context->frameSupportsImagePaintForPaths())
    {
        // Fall back on ImageRectDraw if the current frame doesn't support
        // drawing paths with image paints.
        if (!m_stack.back().clipIsEmpty)
        {
            const Mat2D& m = m_stack.back().matrix;
            auto riveRenderImage =
                static_cast<const RiveRenderImage*>(renderImage);
            clipAndPushDraw(
                gpu::DrawUniquePtr(m_context->make<gpu::ImageRectDraw>(
                    m_context,
                    m.mapBoundingBox(AABB{0, 0, 1, 1}).roundOut(),
                    m,
                    blendMode,
                    riveRenderImage->refTexture(),
                    opacity)));
        }
    }
    else
    {
        // Implement drawImage() as drawPath() with a rectangular path and an
        // image paint.
        if (m_unitRectPath == nullptr)
        {
            m_unitRectPath = make_rcp<RiveRenderPath>();
            m_unitRectPath->line({1, 0});
            m_unitRectPath->line({1, 1});
            m_unitRectPath->line({0, 1});
        }

        RiveRenderPaint paint;
        paint.image(image->refTexture(), opacity);
        paint.blendMode(blendMode);
        drawPath(m_unitRectPath.get(), &paint);
    }

    restore();
}

void RiveRenderer::drawImageMesh(const RenderImage* renderImage,
                                 rcp<RenderBuffer> vertices_f32,
                                 rcp<RenderBuffer> uvCoords_f32,
                                 rcp<RenderBuffer> indices_u16,
                                 uint32_t vertexCount,
                                 uint32_t indexCount,
                                 BlendMode blendMode,
                                 float opacity)
{
    LITE_RTTI_CAST_OR_RETURN(image, const RiveRenderImage*, renderImage);
    const gpu::Texture* texture = image->getTexture();

    assert(vertices_f32);
    assert(uvCoords_f32);
    assert(indices_u16);

    if (m_stack.back().clipIsEmpty)
    {
        return;
    }

    clipAndPushDraw(gpu::DrawUniquePtr(
        m_context->make<gpu::ImageMeshDraw>(gpu::Draw::kFullscreenPixelBounds,
                                            m_stack.back().matrix,
                                            blendMode,
                                            ref_rcp(texture),
                                            std::move(vertices_f32),
                                            std::move(uvCoords_f32),
                                            std::move(indices_u16),
                                            indexCount,
                                            opacity)));
}

void RiveRenderer::clipAndPushDraw(gpu::DrawUniquePtr draw)
{
    assert(!m_stack.back().clipIsEmpty);
    if (draw.get() == nullptr)
    {
        return;
    }
    if (m_context->isOutsideCurrentFrame(draw->pixelBounds()))
    {
        return;
    }

    // Make two attempts to issue the draw: once on the context as-is and once
    // with a clean flush.
    for (int i = 0; i < 2; ++i)
    {
        // Always make sure we begin this loop with the internal draw batch
        // empty, and clear it when we're done.
        struct AutoResetInternalDrawBatch
        {
        public:
            AutoResetInternalDrawBatch(RiveRenderer* renderer) :
                m_renderer(renderer)
            {
                assert(m_renderer->m_internalDrawBatch.empty());
            }
            ~AutoResetInternalDrawBatch()
            {
                m_renderer->m_internalDrawBatch.clear();
            }

        private:
            RiveRenderer* m_renderer;
        };

        AutoResetInternalDrawBatch aridb(this);

        auto applyClipResult = applyClip(draw.get());
        if (applyClipResult == ApplyClipResult::failure)
        {
            // There wasn't room in the GPU buffers for this path draw. Flush
            // and try again.
            m_context->logicalFlush();
            continue;
        }
        else if (applyClipResult == ApplyClipResult::clipEmpty)
        {
            return;
        }

        m_internalDrawBatch.push_back(std::move(draw));
        if (!m_context->pushDraws(m_internalDrawBatch.data(),
                                  m_internalDrawBatch.size()))
        {
            // There wasn't room in the GPU buffers for this path draw. Flush
            // and try again.
            m_context->logicalFlush();
            // Reclaim "draw" because we will use it again on the next
            // iteration.
            draw = std::move(m_internalDrawBatch.back());
            assert(draw != nullptr);
            m_internalDrawBatch.pop_back();
            continue;
        }

        // Success!
        return;
    }

    // We failed to process the draw. Release its refs.
    fprintf(stderr,
            "RiveRenderer::clipAndPushDraw failed. The draw and/or clip stack "
            "are too complex.\n");
}

RiveRenderer::ApplyClipResult RiveRenderer::applyClip(gpu::Draw* draw)
{
    if (m_stack.back().clipIsEmpty)
    {
        return ApplyClipResult::clipEmpty;
    }
    draw->setClipRect(m_stack.back().clipRectInverseMatrix);

    const size_t clipStackHeight = m_stack.back().clipStackHeight;
    if (clipStackHeight == 0)
    {
        assert(draw->clipID() == 0);
        return ApplyClipResult::success;
    }

    // Find which clip element in the stack (if any) is currently rendered to
    // the clip buffer.
    size_t clipIdxCurrentlyInClipBuffer = -1; // i.e., "none".
    if (m_context->getClipContentID() != 0)
    {
        for (size_t i = clipStackHeight - 1; i != -1; --i)
        {
            if (m_clipStack[i].clipID == m_context->getClipContentID())
            {
                clipIdxCurrentlyInClipBuffer = i;
                break;
            }
        }
    }

    // Draw the necessary updates to the clip buffer (i.e., draw every clip
    // element after clipIdxCurrentlyInClipBuffer).
    uint32_t lastClipID =
        clipIdxCurrentlyInClipBuffer == -1
            ? 0 // The next clip to be drawn is not nested.
            : m_clipStack[clipIdxCurrentlyInClipBuffer].clipID;
    if (m_context->frameInterlockMode() == gpu::InterlockMode::msaa)
    {
        if (lastClipID == 0 && m_context->getClipContentID() != 0)
        {
            // Time for a new stencil clip! Erase the clip currently in the
            // stencil buffer before we draw the new one.
            auto stencilClipClear =
                gpu::DrawUniquePtr(m_context->make<gpu::StencilClipReset>(
                    m_context,
                    m_context->getClipContentID(),
                    gpu::StencilClipReset::ResetAction::clearPreviousClip));
            if (!m_context->isOutsideCurrentFrame(
                    stencilClipClear->pixelBounds()))
            {
                m_internalDrawBatch.push_back(std::move(stencilClipClear));
            }
        }
    }

    for (size_t i = clipIdxCurrentlyInClipBuffer + 1; i < clipStackHeight; ++i)
    {
        ClipElement& clip = m_clipStack[i];
        assert(clip.pathBounds == clip.path->getBounds());

        IAABB clipDrawBounds;
        {
            RiveRenderPaint clipUpdatePaint;
            clipUpdatePaint.clipUpdate(
                /*clip THIS clipDraw against:*/ lastClipID);

            gpu::DrawUniquePtr clipDraw =
                clip.path->getDrawCache(clip.matrix,
                                        &clipUpdatePaint,
                                        clip.fillRule,
                                        &m_context->perFrameAllocator(),
                                        m_context->frameDescriptor(),
                                        m_context->frameInterlockMode());

            if (clipDraw == nullptr)
            {
                clipDraw = gpu::RiveRenderPathDraw::Make(m_context,
                                                         clip.matrix,
                                                         clip.path,
                                                         clip.fillRule,
                                                         &clipUpdatePaint,
                                                         &m_scratchPath);

                if (clipDraw == nullptr)
                {
                    return ApplyClipResult::clipEmpty;
                }

                clip.path->setDrawCache(
                    static_cast<gpu::RiveRenderPathDraw*>(clipDraw.get()),
                    clip.matrix,
                    &clipUpdatePaint);
            }

            clipDrawBounds = clipDraw->pixelBounds();
            // Generate a new clipID every time we (re-)render an element to the
            // clip buffer. (Each embodiment of the element needs its own
            // separate readBounds.)
            clip.clipID = m_context->generateClipID(clipDrawBounds);
            assert(clip.clipID != m_context->getClipContentID());
            if (clip.clipID == 0)
            {
                return ApplyClipResult::failure; // The context is out of
                                                 // clipIDs. We will flush and
                                                 // try again.
            }
            clipDraw->setClipID(clip.clipID);
            if (!m_context->isOutsideCurrentFrame(clipDrawBounds))
            {
                m_internalDrawBatch.push_back(std::move(clipDraw));
            }
        }

        if (lastClipID != 0)
        {
            m_context->addClipReadBounds(lastClipID, clipDrawBounds);
            if (m_context->frameInterlockMode() == gpu::InterlockMode::msaa)
            {
                // When drawing nested stencil clips, we need to intersect them,
                // which involves erasing the region of the current clip in the
                // stencil buffer that is outside the the one we just drew.
                auto stencilClipIntersect =
                    gpu::DrawUniquePtr(m_context->make<gpu::StencilClipReset>(
                        m_context,
                        lastClipID,
                        gpu::StencilClipReset::ResetAction::
                            intersectPreviousClip));
                if (!m_context->isOutsideCurrentFrame(
                        stencilClipIntersect->pixelBounds()))
                {
                    m_internalDrawBatch.push_back(
                        std::move(stencilClipIntersect));
                }
            }
        }

        lastClipID = clip.clipID; // Nest the next clip (if any) inside the one
                                  // we just rendered.
    }
    assert(lastClipID == m_clipStack[clipStackHeight - 1].clipID);
    draw->setClipID(lastClipID);
    m_context->addClipReadBounds(lastClipID, draw->pixelBounds());
    m_context->setClipContentID(lastClipID);
    return ApplyClipResult::success;
}
} // namespace rive
