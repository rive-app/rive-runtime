#include "rive/shapes/paint/artboard_paint_source.hpp"
#include "rive/animation/linear_animation_instance.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/artboard.hpp"
#include "rive/data_bind/data_context.hpp"
#include "rive/renderer.hpp"
#ifdef RIVE_CANVAS
#include "rive/offscreen_raster.hpp"
#include "rive/renderer/cmd/deferred_canvas_host.hpp"
#include "rive/renderer/render_canvas.hpp"
#endif

#include <algorithm>
#include <cmath>
#include <limits>

using namespace rive;

namespace
{
// Instancing a source instances the paint sources inside it, so a reference
// cycle recurses without end. The importer cuts every cycle it can see, but
// the editor splices artboards in after import, and a cycle closed by the
// spliced artboard is never re-checked -- so this is the backstop.
constexpr int kMaxNesting = 16;
thread_local int s_nesting = 0;

#ifdef RIVE_CANVAS
// Texels a raster needs along one axis: [span] device pixels covering
// [extent] artboard units. The per-unit scale is quantized up to sixteenths,
// as planRasterScale does, so a zoom or a scale tween does not re-raster at a
// new size on every frame.
//
// Stays a float: an extreme zoom can ask for more texels than a uint32_t
// holds, and converting that is undefined (x86 wraps it to an arbitrary,
// often tiny size that slips under the cap). The caller caps first, then
// converts. An overflow to infinity comes back as the largest float, so the
// cap can still shrink it.
float texelsFor(float span, float extent)
{
  if (!(span > 0.0f) || !std::isfinite(span))
  {
    return 1.0f;
  }
  constexpr float kScaleQuantum = 16.0f;
  const float perUnit = std::ceil(span / extent * kScaleQuantum) /
                        kScaleQuantum;
  return std::min(
      std::max(1.0f, std::ceil(extent * perUnit)),
      std::numeric_limits<float>::max());
}
#endif
} // namespace

ArtboardPaintSource::ArtboardPaintSource(
    std::unique_ptr<ArtboardInstance> artboard)
    : m_artboard(std::move(artboard))
{}

// Out of line so the scene instances and the rcp<gpu::RenderCanvas> are only
// destroyed where their types are complete.
ArtboardPaintSource::~ArtboardPaintSource() = default;

std::unique_ptr<ArtboardPaintSource> ArtboardPaintSource::make(
    const Artboard* source,
    Factory* factory,
    ArtboardHost* host)
{
  if (source == nullptr || s_nesting >= kMaxNesting)
  {
    return nullptr;
  }
  s_nesting++;
  std::unique_ptr<ArtboardInstance>
      instance = source->instance<ArtboardInstance>(factory);
  s_nesting--;
  if (instance == nullptr)
  {
    return nullptr;
  }

  if (host != nullptr)
  {
    instance->host(host);
  }

  std::unique_ptr<ArtboardPaintSource> paintSource(
      new ArtboardPaintSource(std::move(instance)));
  ArtboardInstance* artboard = paintSource->m_artboard.get();
  // The order ArtboardInstance::defaultScene picks in, kept typed so advance
  // can apply NestedArtboard's per-kind rules.
  paintSource->m_stateMachine = artboard->defaultStateMachine();
  if (paintSource->m_stateMachine == nullptr)
  {
    paintSource->m_stateMachine = artboard->stateMachineAt(0);
  }
  if (paintSource->m_stateMachine == nullptr)
  {
    paintSource->m_animation = artboard->animationAt(0);
  }
  // Settle the first frame so the first raster shows the initial state
  // rather than the setup pose.
  paintSource->advance(
      0.0f,
      AdvanceFlags::AdvanceNested | AdvanceFlags::Animate |
          AdvanceFlags::NewFrame);
  paintSource->update();
  return paintSource;
}

bool ArtboardPaintSource::advance(float elapsedSeconds, AdvanceFlags flags)
{
  bool keepGoing = false;
  const bool advanceNested = (flags & AdvanceFlags::AdvanceNested) ==
                             AdvanceFlags::AdvanceNested;
  const bool newFrame = (flags & AdvanceFlags::NewFrame) ==
                        AdvanceFlags::NewFrame;
  if (advanceNested)
  {
    if (m_stateMachine != nullptr)
    {
      // As NestedArtboard does: off a new frame, only advance once the
      // state actually changed, so intermediate values that settle back
      // where they were do not dirty the artboard.
      if (newFrame || m_stateMachine->tryChangeState())
      {
        if (m_stateMachine->advance(elapsedSeconds, newFrame))
        {
          keepGoing = true;
        }
      }
    }
    else if (m_animation != nullptr && newFrame)
    {
      if (m_animation->advance(elapsedSeconds))
      {
        keepGoing = true;
      }
      m_animation->apply();
    }
  }
  if (m_artboard->advanceInternal(
          elapsedSeconds,
          flags & ~AdvanceFlags::IsRoot))
  {
    keepGoing = true;
  }
  return keepGoing;
}

bool ArtboardPaintSource::needsUpdate() const
{
  return m_artboard->hasDirt(ComponentDirt::Components);
}

void ArtboardPaintSource::update() { m_artboard->updatePass(false); }

AABB ArtboardPaintSource::bounds() const { return m_artboard->bounds(); }

void ArtboardPaintSource::dataContext(rcp<DataContext> dataContext)
{
  if (dataContext == nullptr)
  {
    clearDataContext();
    return;
  }
  // The non-stateful, no-globals path of NestedArtboard::internalDataContext:
  // the instance shares its host's context rather than getting its own.
  m_artboard->internalDataContext(dataContext);
  if (m_stateMachine != nullptr)
  {
    m_stateMachine->dataContext(dataContext);
  }
}

void ArtboardPaintSource::relinkDataContext(
    rcp<ViewModelInstance> viewModelInstance)
{
  // Mirrors NestedArtboard::relinkDataContext. With a passed-through context
  // the instance's context is the host's own, so its main instance already
  // matches and only the binds below need relinking.
  if (auto dataContext = m_artboard->dataContext())
  {
    if (dataContext->mainViewModelInstance() != viewModelInstance)
    {
      dataContext->viewModelInstance(viewModelInstance);
    }
  }
  m_artboard->relinkDataContext();
}

void ArtboardPaintSource::clearDataContext()
{
  m_artboard->clearDataContext();
  if (m_stateMachine != nullptr)
  {
    m_stateMachine->clearDataContext();
  }
}

void ArtboardPaintSource::unbind() { m_artboard->unbind(); }

void ArtboardPaintSource::updateDataBinds() { m_artboard->updateDataBinds(); }

#ifdef RIVE_CANVAS
bool ArtboardPaintSource::rasterize(cmd::DeferredCanvasHost* host)
{
  Renderer* renderer = host->beginCanvasContent(m_canvas.get(), 0);
  if (renderer == nullptr)
  {
    // The host could not open an offscreen frame, so the canvas holds
    // whatever it held before (uninitialized on a first attempt). Drop it
    // and stay dirty: the next draw retries instead of sampling a hole.
    m_canvas.reset();
    m_widthPx = 0;
    m_heightPx = 0;
    m_dirty = true;
    return false;
  }
  // Stretch the artboard's box over the whole canvas rather than drawing at
  // a uniform scale: the paint maps the image's 0..1 space onto the shape,
  // so the texture's edges have to be exactly the box's edges, or tiling
  // would repeat the slack that rounding (and a held, larger allocation)
  // leaves at the far side.
  const AABB box = bounds();
  renderer->save();
  renderer->transform(Mat2D::fromScale(
      (float)m_widthPx / box.width(),
      (float)m_heightPx / box.height()));
  renderer->translate(-box.left(), -box.top());
  // drawContent, not draw(): draw() is the root entry point, which advances
  // the frame id and can divert to the watermark.
  m_artboard->drawContent(renderer);
  renderer->restore();
  host->endCanvasContent(m_canvas.get());
  m_dirty = false;
#ifdef TESTING
  m_rasterCount++;
#endif
  return true;
}
#endif

bool ArtboardPaintSource::applyTo(
    Renderer* renderer,
    cmd::DeferredCanvasHost* host,
    RenderPaint* paint,
    ImageSampler sampler,
    const Mat2D& imageTransform,
    ModulatedDrawTarget* target)
{
  target->renderer = renderer;
#ifdef RIVE_CANVAS
  if (host == nullptr)
  {
    return false;
  }
  const AABB box = bounds();
  if (!(box.width() > 0.0f) || !(box.height() > 0.0f))
  {
    return false;
  }

  // How many device pixels the image's unit square spans along each of its
  // axes: the renderer's transform (zoom, density, mount) times the paint's
  // own fit, scale and rotation. A renderer that reports no transform counts
  // as identity, which rasterizes at one texel per local unit.
  Mat2D ctm;
  const bool haveCtm = renderer->currentTransform(&ctm);
  const Mat2D toDevice = haveCtm ? ctm * imageTransform : imageTransform;
  float wantW = texelsFor(
      Vec2D(toDevice.xx(), toDevice.xy()).length(),
      box.width());
  float wantH = texelsFor(
      Vec2D(toDevice.yx(), toDevice.yy()).length(),
      box.height());
  // Cap by shrinking both axes together, so a capped raster keeps the
  // proportions it would have had. Done in float, before converting, since
  // the request can exceed what a uint32_t holds.
  const float maxDim = (float)offscreen::kMaxDim;
  const float largest = std::max(wantW, wantH);
  if (largest > maxDim)
  {
    const float shrink = maxDim / largest;
    wantW = std::min(wantW * shrink, maxDim);
    wantH = std::min(wantH * shrink, maxDim);
  }
  const uint32_t needW = std::max<uint32_t>(1, (uint32_t)wantW);
  const uint32_t needH = std::max<uint32_t>(1, (uint32_t)wantH);

  // Growth reallocates at once; shrinking waits out a streak, so a source
  // animating through a size does not mint a texture per frame. A held
  // allocation larger than needed is still exact, because the content is
  // stretched over whatever the canvas is.
  const offscreen::RasterResize resize = offscreen::decideRasterResize(
      needW,
      needH,
      m_canvas != nullptr ? m_widthPx : 0,
      m_canvas != nullptr ? m_heightPx : 0,
      &m_shrinkStreak);
  if (resize.reallocate || m_canvas == nullptr)
  {
    m_canvas = host->makeContentCanvas(resize.widthPx, resize.heightPx);
    m_widthPx = m_canvas != nullptr ? resize.widthPx : 0;
    m_heightPx = m_canvas != nullptr ? resize.heightPx : 0;
    // A new texture holds nothing.
    m_dirty = true;
  }
  if (m_canvas == nullptr)
  {
    return false;
  }

  if ((m_dirty || m_artboard->didChange()) && !rasterize(host))
  {
    return false;
  }

  // Not necessarily the canvas's own image: some backends cannot sample
  // their canvas textures directly and stand in a companion for it.
  rcp<RenderImage> image = host->contentCanvasImage(m_canvas.get());
  if (image == nullptr)
  {
    return false;
  }

  // An immediate host interrupted the renderer to rasterize, and that
  // renderer can no longer sample the canvas; draw through a clean one, as
  // offscreen::beginComposite does for a cached artboard.
  float opacity = 1.0f;
  if (haveCtm && renderer->currentModulatedOpacity(&opacity))
  {
    if (Renderer* fresh = host->compositeRenderer())
    {
      target->renderer = fresh;
      target->transform = ctm;
      target->opacity = opacity;
    }
  }
  paint->modulatedImage(image.get(), sampler, imageTransform);
  return true;
#else
  // Offscreen canvases are compiled out; the paint draws without an image.
  (void)host;
  (void)paint;
  (void)sampler;
  (void)imageTransform;
  return false;
#endif
}

#ifdef TESTING
uint32_t ArtboardPaintSource::rasterWidth() const
{
#ifdef RIVE_CANVAS
  return m_canvas != nullptr ? m_widthPx : 0;
#else
  return 0;
#endif
}

uint32_t ArtboardPaintSource::rasterHeight() const
{
#ifdef RIVE_CANVAS
  return m_canvas != nullptr ? m_heightPx : 0;
#else
  return 0;
#endif
}
#endif
