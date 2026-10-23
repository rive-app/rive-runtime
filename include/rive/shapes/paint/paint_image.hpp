#ifndef _RIVE_PAINT_IMAGE_HPP_
#define _RIVE_PAINT_IMAGE_HPP_
#include "rive/generated/shapes/paint/paint_image_base.hpp"
#include "rive/shapes/paint/image_sampler.hpp"
#include "rive/advancing_component.hpp"
#include "rive/artboard_host.hpp"
#include "rive/artboard_referencer.hpp"
#include "rive/assets/file_asset_referencer.hpp"
#include "rive/math/aabb.hpp"
#include "rive/math/mat2d.hpp"
#include <memory>

namespace rive
{
class ArtboardPaintSource;
class RenderPaint;
class Renderer;
class ImageAsset;
struct ModulatedDrawTarget;

/// Optional child of a ShapePaint that modulates (multiplies) the parent
/// paint's color/gradient with an image when painting the path. Discovered by
/// the parent via ContainerComponent::firstChild<PaintImage>() (no back-pointer
/// is stored on the ShapePaint).
///
/// The image is either an image asset or an artboard. An artboard source is
/// instanced per paint instance, advanced on its default state machine, and
/// rasterized into an offscreen canvas whenever it changes; it takes
/// precedence over the asset when it resolves. The source binds to the data
/// context of the artboard the paint is in, which reaches it through this
/// paint as an ArtboardHost.
class PaintImage : public PaintImageBase,
                   public FileAssetReferencer,
                   public ArtboardReferencer,
                   public ArtboardHost,
                   public AdvancingComponent
{
public:
  // Out of line so the unique_ptr<ArtboardPaintSource> is only destroyed
  // where its type is complete.
  PaintImage();
  ~PaintImage() override;

  StatusCode import(ImportStack& importStack) override;
  bool validate(CoreContext* context) override;
  StatusCode onAddedClean(CoreContext* context) override;
  void buildDependencies() override;
  void update(ComponentDirt value) override;
  Core* clone() const override;

  // FileAssetReferencer: the image used to modulate the parent paint.
  void setAsset(rcp<FileAsset>) override;
  uint32_t assetId() override;
  void assetUpdated() override;
  ImageAsset* imageAsset() const;
  ImageSampler imageSampler() const;

  // ArtboardReferencer: the artboard used to modulate the parent paint. The
  // referenced artboard is always the file level source; each instance of
  // the host owns its own instance of it.
  void updateArtboard(ViewModelInstanceArtboard*) override {}
  int referencedArtboardId() override { return (int)artboardId(); }
  // So the importer's cycle cut covers paint sources as well as nested
  // artboards.
  Artboard* nestingArtboard() override { return artboard(); }
  Artboard* sourceArtboard() const { return m_referencedArtboard; }

  // AdvancingComponent: plays the artboard source. An image source never
  // advances, so it never keeps a list row from going quiet.
  bool advanceComponent(
      float elapsedSeconds,
      AdvanceFlags flags = AdvanceFlags::Animate |
                           AdvanceFlags::NewFrame) override;
  QuietState quietState() override
  {
    return m_source != nullptr ? QuietState::never : QuietState::quiet;
  }

  // ArtboardHost: the artboard's data context reaches the source through
  // here, passed through unchanged as a NestedArtboard with no bind path
  // does. Only a paint with an artboard source registers as a host.
  size_t artboardCount() override { return m_source != nullptr ? 1 : 0; }
  ArtboardInstance* artboardInstance(int index = 0) override;
  void internalDataContext(rcp<DataContext> dataContext) override;
  void bindViewModelInstance(
      rcp<ViewModelInstance> viewModelInstance,
      rcp<DataContext> parent) override;
  void relinkDataContext(rcp<ViewModelInstance> viewModelInstance) override;
  void clearDataContext() override;
  void unbind() override;
  void updateDataBinds() override;
  Artboard* parentArtboard() override { return artboard(); }
  // Nothing hit tests or lays out through a paint.
  bool hitTestHost(const Vec2D&, bool, ArtboardInstance*) override
  {
    return false;
  }
  Vec2D hostTransformPoint(const Vec2D& point, ArtboardInstance*) override
  {
    return point;
  }
  Mat2D worldTransformForArtboard(ArtboardInstance*) override
  {
    return Mat2D();
  }
  void markHostTransformDirty() override {}
  void file(File* value) override { m_file = value; }
  File* file() const override { return m_file; }
  // hostComponent() stays null on purpose: focus, semantics and parent
  // traversal cross into a host through it, and the source is a picture of
  // an artboard, not part of the hierarchy it is painted in.
  int type() const override { return coreType(); }

  /// Modulate [paint] with this image, auto-fit to [bounds] (the shape's
  /// local bounds) and adjusted by the image transform (scale/offset/rotation
  /// about the bounds center). Returns false (leaving [paint] untouched) when
  /// no image is resolved -- the caller owns clearing any stale image, as it
  /// also has to handle this component being removed entirely.
  ///
  /// [renderer] is what the path is about to be drawn with: an artboard
  /// source sizes its raster from its transform. [target] receives the
  /// renderer the path has to be drawn through instead, if any.
  bool applyTo(
      Renderer* renderer,
      RenderPaint* paint,
      const AABB& bounds,
      ModulatedDrawTarget* target);

  /// Maps the image's normalized 0..1 space onto [bounds]. The base fits the
  /// image across the bounds once ("fill") or at [originalWidth] x
  /// [originalHeight] local units ("original"); the user transform
  /// (scale/offset/rotation about the bounds center) modifies it.
  Mat2D textureTransform(
      const AABB& bounds,
      float originalWidth,
      float originalHeight) const;

#ifdef TESTING
  ArtboardPaintSource* artboardSource() const { return m_source.get(); }
#endif

private:
  void invalidatePaint();

  std::unique_ptr<ArtboardPaintSource> m_source;
  File* m_file = nullptr;
};
} // namespace rive

#endif
