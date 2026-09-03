#ifndef _RIVE_WATERMARK_HPP_
#define _RIVE_WATERMARK_HPP_

#include "rive/math/aabb.hpp"
#include "rive/refcnt.hpp"

#include <cstdint>
#include <memory>

namespace rive
{
class ArtboardInstance;
class RenderPaint;
class RenderPath;
class Renderer;
class StateMachineInstance;

/// A pre-roll played in front of the artboard instance it is attached to. While
/// it plays, the host artboard neither animates nor draws: the watermark is the
/// only thing advanced and the only thing on screen. Once it finishes the host
/// takes over and the watermark is released, so it never plays twice for the
/// same instance.
///
/// Attached by File to the instances it vends when the file's manifest carries
/// a watermark section (see ManifestAsset::hasWatermark()).
class Watermark
{
public:
    Watermark(std::unique_ptr<ArtboardInstance> artboard,
              std::unique_ptr<StateMachineInstance> stateMachine);
    ~Watermark();

    /// Whether this frame belongs to the watermark rather than to the host.
    /// True from the first advance until the owner releases the watermark,
    /// which it does on the frame after the last one the watermark played. The
    /// "has it started" part matters: a host that is never driven by a state
    /// machine never starts its watermark, and must keep drawing itself rather
    /// than freeze on a pre-roll that would never end.
    bool isPlaying() const { return m_started; }

    /// How much of the pre-roll has actually been played. This is what the
    /// clock budget bounds, so it is the thing worth asserting on.
    float elapsedSeconds() const { return m_elapsedSeconds; }

    /// Advance the pre-roll. Returns true while the frame belongs to the
    /// watermark, meaning the host must stay frozen. The frame on which the
    /// watermark finishes still returns true, so its last authored frame gets
    /// drawn; the handover happens on the next one.
    ///
    /// elapsedSeconds is a request rather than an instruction, see
    /// clampElapsed().
    bool advance(float elapsedSeconds);

    /// Draw the watermark over an opaque backdrop covering the host's box.
    ///
    /// The backdrop is not decoration. The host is frozen but whatever it last
    /// drew is still on the surface, and the watermark is fit inside the host's
    /// box rather than filling it, so without one the file would show through
    /// and around the pre-roll.
    void draw(Renderer* renderer, const AABB& hostBounds);

private:
    /// How much of elapsedSeconds the pre-roll actually consumes.
    ///
    /// The host's delta is honored while the pre-roll's total consumption
    /// stays within the real time that has actually passed (plus one small
    /// tolerance), so playback stays smooth and a host with a better notion of
    /// frame time (fixed timestep, vsync) still drives it. Beyond that it is
    /// cut to what is left of the budget, which is how a watermark would
    /// otherwise get skipped: one huge advance to seek past it, or a loop
    /// advancing far faster than it draws. The budget is cumulative
    /// deliberately -- a per-call comparison lets the tolerance be re-spent
    /// every frame, so a loop of advances just under it costs nothing in real
    /// time yet finishes the pre-roll.
    ///
    /// Under File::deterministicMode the host owns the timeline outright and
    /// elapsedSeconds is taken verbatim, matching how ScrollPhysics and
    /// TextInputListenerGroup treat time.
    float clampElapsed(float elapsedSeconds);

    /// Fill [hostBounds] with the opaque backdrop, building (and caching) the
    /// paint and path on first use.
    void drawBackdrop(Renderer* renderer, const AABB& hostBounds);

    // Declared before m_stateMachine so it outlives it: Scene holds a raw,
    // non-owning ArtboardInstance*, and members are destroyed in reverse.
    std::unique_ptr<ArtboardInstance> m_artboard;
    std::unique_ptr<StateMachineInstance> m_stateMachine;
    // Monotonic clock in microseconds, sampled at construction and at every
    // advance.
    int64_t m_lastTimeMicros;
    // Real time measured across every advance. Together with m_elapsedSeconds
    // it is the pre-roll's spending limit: what it has consumed may not run
    // ahead of what has actually elapsed, give or take one tolerance.
    float m_wallSeconds = 0.0f;
    // Backdrop paint and path, built lazily on the first draw. The path is
    // rebuilt whenever the host's box changes, since a window can be resized
    // mid pre-roll.
    rcp<RenderPaint> m_backdropPaint;
    rcp<RenderPath> m_backdropPath;
    AABB m_backdropBounds;
    float m_elapsedSeconds = 0.0f;
    bool m_started = false;
    bool m_finished = false;
};
} // namespace rive

#endif
