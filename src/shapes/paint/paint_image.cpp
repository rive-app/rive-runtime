#include "rive/shapes/paint/paint_image.hpp"
#include "rive/shapes/paint/artboard_paint_source.hpp"
#include "rive/shapes/paint/shape_paint.hpp"
#include "rive/artboard.hpp"
#include "rive/assets/file_asset.hpp"
#include "rive/assets/image_asset.hpp"
#include "rive/factory.hpp"
#include "rive/importers/backboard_importer.hpp"
#include "rive/importers/import_stack.hpp"
#include "rive/renderer.hpp"
#include "rive/core_context.hpp"

using namespace rive;

PaintImage::PaintImage() = default;
PaintImage::~PaintImage() = default;

StatusCode PaintImage::import(ImportStack& importStack)
{
  // The image is optional, so registration is best-effort: unlike the Image
  // node we must not fail the import when there's no backboard importer. The
  // backboard importer safely skips out-of-range indices during resolve.
  registerReferencer(importStack);
  StatusCode code = Super::import(importStack);
  if (code != StatusCode::Ok)
  {
    return code;
  }
  // Same for the artboard, and registered last, once this object is certain
  // to survive -- resolve also runs the nested artboard cycle cut over it,
  // and skips an id that names no artboard (an image source's).
  if (auto backboardImporter = importStack.latest<BackboardImporter>(
          Backboard::typeKey))
  {
    backboardImporter->addArtboardReferencer(this);
  }
  return StatusCode::Ok;
}

bool PaintImage::validate(CoreContext* context)
{
  if (!Super::validate(context))
  {
    return false;
  }
  auto coreObject = context->resolve(parentId());
  // we know it's not nullptr from Super::validate
  assert(coreObject != nullptr);
  return coreObject->is<ShapePaint>();
}

StatusCode PaintImage::onAddedClean(CoreContext* context)
{
  StatusCode code = Super::onAddedClean(context);
  if (code != StatusCode::Ok)
  {
    return code;
  }
  // Only instances play the source: a file level artboard is never drawn,
  // and instancing here (rather than in clone) picks up the factory the
  // host instance was given, which instance() only sets after cloning.
  Artboard* host = artboard();
  if (m_referencedArtboard != nullptr && host != nullptr && host->isInstance())
  {
    m_source = ArtboardPaintSource::make(
        m_referencedArtboard,
        host->factory(),
        this);
    if (m_source != nullptr)
    {
      addDirt(ComponentDirt::Components);
      // Normally the context arrives later, when the host is bound; an
      // instance made inside an already bound artboard picks it up now.
      if (auto dataContext = host->dataContext())
      {
        m_source->dataContext(dataContext);
      }
    }
  }
  return StatusCode::Ok;
}

ArtboardInstance* PaintImage::artboardInstance(int index)
{
  return index == 0 && m_source != nullptr ? m_source->artboard() : nullptr;
}

void PaintImage::internalDataContext(rcp<DataContext> dataContext)
{
  if (m_source != nullptr)
  {
    m_source->dataContext(dataContext);
  }
}

void PaintImage::bindViewModelInstance(
    rcp<ViewModelInstance>,
    rcp<DataContext> parent)
{
  // Only reached for a bind path, which a paint does not have; keep the
  // passthrough either way.
  internalDataContext(parent);
}

void PaintImage::relinkDataContext(rcp<ViewModelInstance> viewModelInstance)
{
  if (m_source != nullptr)
  {
    m_source->relinkDataContext(viewModelInstance);
  }
}

void PaintImage::clearDataContext()
{
  if (m_source != nullptr)
  {
    m_source->clearDataContext();
  }
}

void PaintImage::unbind()
{
  if (m_source != nullptr)
  {
    m_source->unbind();
  }
}

void PaintImage::updateDataBinds()
{
  if (m_source != nullptr && !isCollapsed())
  {
    m_source->updateDataBinds();
  }
}

void PaintImage::buildDependencies()
{
  Super::buildDependencies();
  // Puts this in the update order, so the Components dirt an advancing
  // source raises reaches update() and gets cleared -- otherwise the dirt
  // sticks, and every later change is swallowed as "already marked". On the
  // artboard rather than the parent paint: a Fill only joins the graph when
  // it has path effects. Unconditional, so an instance's replayed order
  // always matches its source's.
  if (Artboard* owner = artboard())
  {
    owner->addDependent(this);
  }
}

void PaintImage::update(ComponentDirt value)
{
  Super::update(value);
  if (m_source != nullptr && hasDirt(value, ComponentDirt::Components))
  {
    // Whether this updated is discarded, as NestedArtboard does: by the end
    // of the pass all the dirt is gone, and only another advance of the
    // scene can bring it back.
    m_source->update();
  }
}

bool PaintImage::advanceComponent(float elapsedSeconds, AdvanceFlags flags)
{
  if (m_source == nullptr || isCollapsed())
  {
    return false;
  }
  bool keepGoing = m_source->advance(elapsedSeconds, flags);
  if (m_source->needsUpdate())
  {
    // Marks the host artboard changed too, so a host gated on didChange()
    // draws the frame that re-rasterizes the source.
    addDirt(ComponentDirt::Components);
  }
  return keepGoing;
}

Core* PaintImage::clone() const
{
  PaintImage* twin = PaintImageBase::clone()->as<PaintImage>();
  // The generated clone copies the imageAssetId property but not the resolved
  // asset, so carry it over (mirrors Image::clone). Without this a nested
  // artboard instance loses its paint image.
  if (m_fileAsset != nullptr)
  {
    twin->setAsset(m_fileAsset);
  }
  // The file level source, never an instance of it: the twin instances its
  // own once its artboard is initialized (onAddedClean).
  twin->referencedArtboard(m_referencedArtboard);
  return twin;
}

uint32_t PaintImage::assetId() { return imageAssetId(); }

void PaintImage::invalidatePaint()
{
  if (parent() != nullptr && parent()->is<ShapePaint>())
  {
    parent()->as<ShapePaint>()->invalidateRendering();
  }
}

void PaintImage::setAsset(rcp<FileAsset> asset)
{
  if (asset != nullptr && asset->is<ImageAsset>())
  {
    FileAssetReferencer::setAsset(asset);
    invalidatePaint();
  }
}

// The texture usually lands after setAsset -- decoding is async, and in the
// editor the bytes arrive from the CDN later still. ImageAsset publishes it by
// calling assetUpdated() on its referencers, so re-invalidate here; otherwise a
// paint that recorded "no image" on its first draw keeps that cleared state
// until some unrelated redraw happens to dirty it.
void PaintImage::assetUpdated() { invalidatePaint(); }

ImageAsset* PaintImage::imageAsset() const
{
  return static_cast<ImageAsset*>(m_fileAsset.get());
}

ImageSampler PaintImage::imageSampler() const
{
  // Clamp file values so the key stays inside the backends' sampler tables.
  auto filterValue = [](uint32_t value) {
    return value <= (uint32_t)ImageFilter::nearest
               ? static_cast<ImageFilter>(value)
               : ImageFilter::bilinear;
  };
  auto wrapValue = [](uint32_t value) {
    return value <= (uint32_t)ImageWrap::mirror ? static_cast<ImageWrap>(value)
                                                : ImageWrap::clamp;
  };
  ImageSampler sampler = ImageSampler::LinearClamp();
  if (ImageAsset* asset = imageAsset())
  {
    sampler.filter = filterValue(asset->samplerFilter());
    sampler.wrapX = wrapValue(asset->samplerWrapX());
    sampler.wrapY = wrapValue(asset->samplerWrapY());
  }
  // Paint values are offset by one, zero means inherit from the asset.
  if (imageSamplerFilter() != 0)
  {
    sampler.filter = filterValue(imageSamplerFilter() - 1);
  }
  if (imageSamplerWrapX() != 0)
  {
    sampler.wrapX = wrapValue(imageSamplerWrapX() - 1);
  }
  if (imageSamplerWrapY() != 0)
  {
    sampler.wrapY = wrapValue(imageSamplerWrapY() - 1);
  }
  return sampler;
}

Mat2D PaintImage::textureTransform(
    const AABB& bounds,
    float originalWidth,
    float originalHeight) const
{
  // Maps the image's normalized texture space (0..1) into the path's
  // coordinate space. The base fits the image across the bounds once; the
  // user transform (scale/offset/rotation about the bounds center) modifies
  // it. At defaults it reproduces the plain fit; with scale < 1 and
  // wrap=repeat/mirror it tiles. Base tile size: shape bounds (fill) or the
  // source's own size (original). 0 = fill, 1 = original.
  bool original = imageSizeMode() == 1;
  float baseW = original ? originalWidth : bounds.width();
  float baseH = original ? originalHeight : bounds.height();
  float cx = bounds.left() + bounds.width() * 0.5f;
  float cy = bounds.top() + bounds.height() * 0.5f;
  return Mat2D::fromTranslate(cx, cy) * Mat2D::fromRotation(imageRotation()) *
         Mat2D::fromTranslate(imageOffsetX() * baseW, imageOffsetY() * baseH) *
         Mat2D::fromScale(baseW * imageScaleX(), baseH * imageScaleY()) *
         Mat2D::fromTranslate(-0.5f, -0.5f);
}

bool PaintImage::applyTo(
    Renderer* renderer,
    RenderPaint* paint,
    const AABB& bounds,
    ModulatedDrawTarget* target)
{
  target->renderer = renderer;
  if (m_referencedArtboard != nullptr)
  {
    // An artboard source wins over the asset whenever it resolves, even
    // while it has no instance (a file level artboard is never drawn).
    if (m_source == nullptr)
    {
      return false;
    }
    Factory* factory = artboard() != nullptr ? artboard()->factory() : nullptr;
    // "Original" is the artboard's own size in its units, not the raster's
    // pixels: those follow the zoom.
    const AABB box = m_source->bounds();
    return m_source->applyTo(
        renderer,
        factory != nullptr ? factory->canvasContentHost() : nullptr,
        paint,
        imageSampler(),
        textureTransform(bounds, box.width(), box.height()),
        target);
  }

  ImageAsset* asset = imageAsset();
  RenderImage* renderImage = asset != nullptr ? asset->renderImage() : nullptr;
  if (renderImage == nullptr)
  {
    // The asset was cleared (or replaced by one that hasn't decoded yet).
    // The caller clears the stale texture off the (persistent) paint.
    return false;
  }
  paint->modulatedImage(
      renderImage,
      imageSampler(),
      textureTransform(
          bounds,
          (float)renderImage->width(),
          (float)renderImage->height()));
  return true;
}
