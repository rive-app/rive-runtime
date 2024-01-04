/*
 * Copyright 2022 Rive
 */

#include "rive/pls/pls_renderer.hpp"

#include "pls_paint.hpp"
#include "pls_path.hpp"
#include "pls_draw.hpp"
#include "rive/math/math_types.hpp"
#include "rive/math/simd.hpp"
#include "rive/pls/pls_image.hpp"
#include "shaders/constants.glsl"

namespace rive::pls
{
bool PLSRenderer::IsAABB(const RawPath& path, AABB* result)
{
    // Any quadrilateral begins with a move plus 3 lines.
    constexpr static size_t kAABBVerbCount = 4;
    constexpr static PathVerb aabbVerbs[kAABBVerbCount] = {PathVerb::move,
                                                           PathVerb::line,
                                                           PathVerb::line,
                                                           PathVerb::line};
    Span<const PathVerb> verbs = path.verbs();
    if (verbs.count() < kAABBVerbCount || memcmp(verbs.data(), aabbVerbs, sizeof(aabbVerbs)) != 0)
    {
        return false;
    }

    // Only accept extra verbs and points if every point after the quadrilateral is equal to p0.
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
    if (simd::all(corners == oppositeCorners.zyxw) || simd::all(corners == oppositeCorners.xwzy))
    {
        float4 r = simd::join(simd::min(corners.xy, corners.zw), simd::max(corners.xy, corners.zw));
        simd::store(result, r);
        return true;
    }
    return false;
}

PLSRenderer::ClipElement::ClipElement(const Mat2D& matrix_,
                                      const PLSPath* path_,
                                      FillRule fillRule_)
{
    reset(matrix_, path_, fillRule_);
}

PLSRenderer::ClipElement::~ClipElement() {}

void PLSRenderer::ClipElement::reset(const Mat2D& matrix_, const PLSPath* path_, FillRule fillRule_)
{
    matrix = matrix_;
    rawPathMutationID = path_->getRawPathMutationID();
    pathBounds = path_->getBounds();
    path = ref_rcp(path_);
    fillRule = fillRule_;
    clipID = 0; // This gets initialized lazily.
}

bool PLSRenderer::ClipElement::isEquivalent(const Mat2D& matrix_, const PLSPath* path_) const
{
    return matrix_ == matrix && path_->getRawPathMutationID() == rawPathMutationID &&
           path_->getFillRule() == fillRule;
}

PLSRenderer::PLSRenderer(PLSRenderContext* context) : m_context(context) {}

PLSRenderer::~PLSRenderer() {}

void PLSRenderer::save()
{
    // Copy the back of the stack before pushing, in case the vector grows and invalidates the
    // reference.
    RenderState copy = m_stack.back();
    m_stack.push_back(copy);
}

void PLSRenderer::restore()
{
    assert(!m_stack.empty());
    assert(m_stack.back().clipStackHeight >= m_stack.back().clipStackHeight);
    m_stack.pop_back();
}

void PLSRenderer::transform(const Mat2D& matrix)
{
    m_stack.back().matrix = m_stack.back().matrix * matrix;
}

void PLSRenderer::drawPath(RenderPath* renderPath, RenderPaint* renderPaint)
{
    LITE_RTTI_CAST_OR_RETURN(path, PLSPath*, renderPath);
    LITE_RTTI_CAST_OR_RETURN(paint, PLSPaint*, renderPaint);

    bool stroked = paint->getIsStroked();

    if (stroked && m_context->frameDescriptor().strokesDisabled)
    {
        return;
    }
    if (!stroked && m_context->frameDescriptor().fillsDisabled)
    {
        return;
    }
    // A stroke width of zero in PLS means a path is filled.
    if (stroked && paint->getThickness() <= 0)
    {
        return;
    }

    clipAndPushDraw(PLSPathDraw::Make(m_context,
                                      m_stack.back().matrix,
                                      ref_rcp(path),
                                      path->getFillRule(),
                                      paint,
                                      &m_scratchPath));
}

void PLSRenderer::clipPath(RenderPath* renderPath)
{
    LITE_RTTI_CAST_OR_RETURN(path, PLSPath*, renderPath);

    // First try to handle axis-aligned rectangles using the "ENABLE_CLIP_RECT" shader feature.
    // Multiple axis-aligned rectangles can be intersected into a single rectangle if their matrices
    // are compatible.
    AABB clipRectCandidate;
    if (IsAABB(path->getRawPath(), &clipRectCandidate))
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
    float maxSkew = std::max(fabsf(currentToNew.xy()), fabsf(currentToNew.yx()));
    float maxScale = std::max(fabsf(currentToNew.xx()), fabsf(currentToNew.yy()));
    if (maxSkew > math::EPSILON && maxScale > math::EPSILON)
    {
        // Transforming this rect to the new view matrix would turn it into something that isn't a
        // rect.
        return false;
    }
    Vec2D pts[2] = {{rect->left(), rect->top()}, {rect->right(), rect->bottom()}};
    currentToNew.mapPoints(pts, pts, 2);
    float4 p = simd::load4f(pts);
    float2 topLeft = simd::min(p.xy, p.zw);
    float2 botRight = simd::max(p.xy, p.zw);
    *rect = {topLeft.x, topLeft.y, botRight.x, botRight.y};
    return true;
}

void PLSRenderer::clipRectImpl(AABB rect, const PLSPath* originalPath)
{
    bool hasClipRect = m_stack.back().clipRectInverseMatrix != nullptr;

    // If there already is a clipRect, we can only accept another one by intersecting it with the
    // existing one. This means the new rect must be axis-aligned with the existing clipRect.
    if (hasClipRect &&
        !transform_rect_to_new_space(&rect, m_stack.back().matrix, m_stack.back().clipRectMatrix))
    {
        // 'rect' is not axis-aligned with the existing clipRect. Fall back to clipPath.
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
        // Both rects are in the same space now. Intersect the two geometrically.
        float4 a = simd::load4f(&m_stack.back().clipRect);
        float4 b = simd::load4f(&rect);
        float4 intersection = simd::join(simd::max(a.xy, b.xy), simd::min(a.zw, b.zw));
        simd::store(&m_stack.back().clipRect, intersection);
    }

    m_stack.back().clipRectInverseMatrix =
        m_context->make<pls::ClipRectInverseMatrix>(m_stack.back().clipRectMatrix,
                                                    m_stack.back().clipRect);
}

void PLSRenderer::clipPathImpl(const PLSPath* path)
{
    // Only write a new clip element if this path isn't already on the stack from before. e.g.:
    //
    //     clipPath(samePath);
    //     restore();
    //     save();
    //     clipPath(samePath); // <-- reuse the ClipElement (and clipID!) already in m_clipStack.
    //
    const size_t clipStackHeight = m_stack.back().clipStackHeight;
    assert(m_clipStack.size() >= clipStackHeight);
    if (m_clipStack.size() == clipStackHeight ||
        !m_clipStack[clipStackHeight].isEquivalent(m_stack.back().matrix, path))
    {
        m_clipStack.resize(clipStackHeight);
        m_clipStack.emplace_back(m_stack.back().matrix, path, path->getFillRule());
    }
    m_stack.back().clipStackHeight = clipStackHeight + 1;
}

void PLSRenderer::drawImage(const RenderImage* renderImage, BlendMode blendMode, float opacity)
{
    LITE_RTTI_CAST_OR_RETURN(image, const PLSImage*, renderImage);

    // Scale the view matrix so we can draw this image as the rect [0, 0, 1, 1].
    save();
    scale(image->width(), image->height());

    if (m_context->frameDescriptor().enableExperimentalAtomicMode &&
        !m_context->impl()->platformFeatures().supportsBindlessTextures)
    {
        // Fall back on ImageRectDraw if the current frame doesn't support drawing paths with image
        // paints.
        auto plsImage = static_cast<const PLSImage*>(renderImage);
        clipAndPushDraw(m_context->make<ImageRectDraw>(m_context,
                                                       m_stack.back().matrix,
                                                       AABB(),
                                                       blendMode,
                                                       plsImage->refTexture(),
                                                       opacity));
    }
    else
    {
        // Implement drawImage() as drawPath() with a rectangular path and an image paint.
        if (m_unitRectPath == nullptr)
        {
            m_unitRectPath = make_rcp<PLSPath>();
            m_unitRectPath->line({1, 0});
            m_unitRectPath->line({1, 1});
            m_unitRectPath->line({0, 1});
        }

        PLSPaint paint;
        paint.image(image->refTexture(), opacity);
        paint.blendMode(blendMode);
        drawPath(m_unitRectPath.get(), &paint);
    }

    restore();
}

void PLSRenderer::drawImageMesh(const RenderImage* renderImage,
                                rcp<RenderBuffer> vertices_f32,
                                rcp<RenderBuffer> uvCoords_f32,
                                rcp<RenderBuffer> indices_u16,
                                uint32_t vertexCount,
                                uint32_t indexCount,
                                BlendMode blendMode,
                                float opacity)
{
    LITE_RTTI_CAST_OR_RETURN(image, const PLSImage*, renderImage);
    const PLSTexture* plsTexture = image->getTexture();

    assert(vertices_f32);
    assert(uvCoords_f32);
    assert(indices_u16);

    clipAndPushDraw(m_context->make<ImageMeshDraw>(m_stack.back().matrix,
                                                   AABB(),
                                                   blendMode,
                                                   ref_rcp(plsTexture),
                                                   std::move(vertices_f32),
                                                   std::move(uvCoords_f32),
                                                   std::move(indices_u16),
                                                   indexCount,
                                                   opacity));
}

void PLSRenderer::clipAndPushDraw(PLSDraw* draw)
{
    // Make two attempts to issue the draw: once on the context as-is and once with a clean flush.
    for (int i = 0; i < 2; ++i)
    {
        assert(m_internalDrawBatch.empty());
        if (!applyClip(draw))
        {
            // There wasn't room in the GPU buffers for this path draw. Flush and try again.
            m_context->flush(PLSRenderContext::FlushType::intermediate);
            releaseInternalDrawBatch();
            continue;
        }
        m_internalDrawBatch.push_back(draw);
        if (!m_context->pushDrawBatch(m_internalDrawBatch.data(), m_internalDrawBatch.size()))
        {
            // There wasn't room in the GPU buffers for this path draw. Flush and try again.
            m_context->flush(PLSRenderContext::FlushType::intermediate);
            // Don't call releaseRefs() on "draw" because we will use it again next iteration.
            assert(m_internalDrawBatch.back() == draw);
            m_internalDrawBatch.pop_back();
            releaseInternalDrawBatch();
            continue;
        }
        // Success! Don't call releaseRefs() because m_context has adopted the draws.
        m_internalDrawBatch.clear();
        return;
    }
    // We failed to process the draw. Release its refs.
    draw->releaseRefs();
    fprintf(stderr,
            "PLSRenderer::clipAndPushDraw failed. The draw and/or clip stack are too complex.\n");
}

bool PLSRenderer::applyClip(PLSDraw* draw)
{
    draw->setClipRect(m_stack.back().clipRectInverseMatrix);

    const size_t clipStackHeight = m_stack.back().clipStackHeight;
    if (clipStackHeight == 0)
    {
        draw->setClipID(0);
        return true;
    }

    // Generate new clipIDs (definitely causing us to redraw the clip buffer) for each element that
    // hasn't been assigned an ID yet, OR if we are on a new flush. (New flushes invalidate the clip
    // buffer.)
    bool needsClipInvalidate = m_clipStackFlushID != m_context->getFlushCount();
    // Also detect which clip element (if any) is already rendered in the clip buffer.
    size_t clipIdxCurrentlyInClipBuffer = -1; // i.e., "none".
    for (size_t i = 0; i < clipStackHeight; ++i)
    {
        ClipElement& clip = m_clipStack[i];
        if (clip.clipID == 0 || needsClipInvalidate)
        {
            clip.clipID = m_context->generateClipID();
            assert(m_context->getClipContentID() != clip.clipID);
            if (clip.clipID == 0)
            {
                return false; // The context is out of clipIDs. We will flush and try again.
            }
            continue;
        }
        if (clip.clipID == m_context->getClipContentID())
        {
            // This is the clip that's currently drawn in the clip buffer! We should only find a
            // match once, so clipIdxCurrentlyInClipBuffer better be at the initial "none" value at
            // this point.
            assert(clipIdxCurrentlyInClipBuffer == -1);
            clipIdxCurrentlyInClipBuffer = i;
        }
    }

    // Draw the necessary updates to the clip buffer (i.e., draw every clip element after
    // clipIdxCurrentlyInClipBuffer).
    uint32_t outerClipID = clipIdxCurrentlyInClipBuffer == -1
                               ? 0 // The next clip to be drawn is not nested.
                               : m_clipStack[clipIdxCurrentlyInClipBuffer].clipID;
    PLSPaint clipUpdatePaint;
    assert(!clipUpdatePaint.getIsStroked());
    for (size_t i = clipIdxCurrentlyInClipBuffer + 1; i < clipStackHeight; ++i)
    {
        const ClipElement& clip = m_clipStack[i];
        assert(clip.clipID != 0);
        assert(clip.pathBounds == clip.path->getBounds());
        clipUpdatePaint.clipUpdate(outerClipID);
        auto clipDraw = PLSPathDraw::Make(m_context,
                                          clip.matrix,
                                          clip.path,
                                          clip.fillRule,
                                          &clipUpdatePaint,
                                          &m_scratchPath);
        clipDraw->setClipID(clip.clipID);
        m_internalDrawBatch.push_back(clipDraw);
        outerClipID = clip.clipID; // Nest the next clip (if any) inside the one we just rendered.
    }
    uint32_t clipID = m_clipStack[clipStackHeight - 1].clipID;
    draw->setClipID(clipID);
    m_context->setClipContentID(clipID);
    m_clipStackFlushID = m_context->getFlushCount();
    return true;
}

void PLSRenderer::releaseInternalDrawBatch()
{
    for (PLSDraw* draw : m_internalDrawBatch)
    {
        draw->releaseRefs();
    }
    m_internalDrawBatch.clear();
}
} // namespace rive::pls
