#ifndef _RIVE_ARTBOARD_PAINT_SOURCE_HPP_
#define _RIVE_ARTBOARD_PAINT_SOURCE_HPP_
#include "rive/advance_flags.hpp"
#include "rive/math/aabb.hpp"
#include "rive/math/mat2d.hpp"
#include "rive/refcnt.hpp"
#include "rive/shapes/paint/image_sampler.hpp"
#include <cstdint>
#include <memory>

namespace rive
{
class Artboard;
class ArtboardHost;
class ArtboardInstance;
class DataContext;
class Factory;
class LinearAnimationInstance;
class RenderPaint;
class Renderer;
class StateMachineInstance;
class ViewModelInstance;
#ifdef RIVE_CANVAS
namespace gpu
{
class RenderCanvas;
}
#endif
namespace cmd
{
class DeferredCanvasHost;
}

/// Where a path modulated by a paint image has to be drawn. [renderer] is the
/// renderer the draw started on, unless an immediate host interrupted it to
/// rasterize -- that renderer can no longer sample the canvas, so the host
/// hands back a clean one, which has to be given [transform] and [opacity] by
/// hand and inherits no clip (the same limitation a cached artboard has).
struct ModulatedDrawTarget
{
  Renderer* renderer = nullptr;
  Mat2D transform;
  float opacity = 1.0f;
};

/// An artboard used as a paint's image: one instance of the source artboard,
/// the scene that plays it, and the offscreen raster it is drawn into. Owned by
/// a PaintImage at runtime and by the editor's native binding, so both advance
/// and rasterize it the same way.
class ArtboardPaintSource
{
public:
  /// Instances [source] on [factory] and picks what plays it: the default
  /// state machine, else the first state machine, else the first animation,
  /// else nothing (the artboard draws static). Returns null when the source
  /// cannot be instanced, including past the nesting depth that guards
  /// against a reference cycle the importer could not see.
  ///
  /// [host] (optional) becomes the instance's host, so a change inside it
  /// reaches the artboard the paint is in (Artboard::changed walks hosts).
  static std::unique_ptr<ArtboardPaintSource> make(
      const Artboard* source,
      Factory* factory,
      ArtboardHost* host = nullptr);
  ~ArtboardPaintSource();

  ArtboardPaintSource(const ArtboardPaintSource&) = delete;
  ArtboardPaintSource& operator=(const ArtboardPaintSource&) = delete;

  /// Advances the scene and the instance's own advancing components, by the
  /// same rules NestedArtboard applies to what it hosts. Returns whether
  /// anything wants to keep advancing.
  bool advance(float elapsedSeconds, AdvanceFlags flags);

  /// Whether the last advance left the instance needing an update pass.
  bool needsUpdate() const;
  void update();

  /// The instance's own box: what gets rasterized, and the logical size the
  /// paint's "original" size mode maps one artboard unit from.
  AABB bounds() const;

  ArtboardInstance* artboard() const { return m_artboard.get(); }

  /// Binds the instance and its scene to [dataContext]: the context of the
  /// artboard the paint is in, passed straight through the way a
  /// NestedArtboard with no bind path passes its host's. The source's binds
  /// resolve against it, so they find the host's view model, or any view
  /// model reachable from it.
  void dataContext(rcp<DataContext> dataContext);
  void relinkDataContext(rcp<ViewModelInstance> viewModelInstance);
  void clearDataContext();
  void unbind();
  void updateDataBinds();

  /// Rasterizes the artboard (when it changed, or the raster no longer fits
  /// how it is viewed) into an offscreen canvas from [host], and installs
  /// that canvas as [paint]'s modulated image. [imageTransform] maps the
  /// image's normalized 0..1 space into the space [renderer] is drawing the
  /// path in, exactly as for an image asset.
  ///
  /// Returns false, leaving [paint] untouched, when nothing could be
  /// rasterized -- no host, or the host could not open a frame. The caller
  /// clears any stale image, as it does for an undecoded asset.
  ///
  /// [target] receives the renderer the path has to be drawn through.
  bool applyTo(
      Renderer* renderer,
      cmd::DeferredCanvasHost* host,
      RenderPaint* paint,
      ImageSampler sampler,
      const Mat2D& imageTransform,
      ModulatedDrawTarget* target);

#ifdef TESTING
  uint32_t rasterWidth() const;
  uint32_t rasterHeight() const;
  uint32_t rasterCount() const { return m_rasterCount; }
  bool hasStateMachine() const { return m_stateMachine != nullptr; }
  bool hasAnimation() const { return m_animation != nullptr; }
#endif

private:
  explicit ArtboardPaintSource(std::unique_ptr<ArtboardInstance> artboard);

#ifdef RIVE_CANVAS
  bool rasterize(cmd::DeferredCanvasHost* host);
#endif

  // Declared first so it is destroyed last: the scene instances below hold
  // pointers into it.
  std::unique_ptr<ArtboardInstance> m_artboard;
  std::unique_ptr<StateMachineInstance> m_stateMachine;
  std::unique_ptr<LinearAnimationInstance> m_animation;
#ifdef RIVE_CANVAS
  rcp<gpu::RenderCanvas> m_canvas;
  uint32_t m_widthPx = 0;
  uint32_t m_heightPx = 0;
  // Consecutive draws that wanted a smaller raster; see decideRasterResize.
  uint8_t m_shrinkStreak = 0;
  bool m_dirty = true;
#endif
#ifdef TESTING
  uint32_t m_rasterCount = 0;
#endif
};
} // namespace rive

#endif
