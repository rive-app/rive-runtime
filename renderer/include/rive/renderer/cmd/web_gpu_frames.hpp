/*
 * Copyright 2026 Rive
 */

#pragma once

#if defined(__EMSCRIPTEN_PTHREADS__)

#include "rive/refcnt.hpp"
#include <GLES3/gl3.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <deque>
#include <functional>
#include <utility>
#include <emscripten/eventloop.h>
#include <emscripten/html5.h>

namespace rive::cmd
{

// Frames the replay worker submitted that the GPU has yet to run. Replay only
// queues GL commands, so a frame holds its slot until the GPU process has run
// them, and how long that takes is what the page paces to. Worker only, except
// replayMs.
class WebGPUFrames : public RefCnt<WebGPUFrames>
{
public:
  // How long recent frames took to finish, 0 before the first.
  float replayMs() const { return m_replayMs.load(std::memory_order_relaxed); }

  // Fences what the worker queued on contextGL since submitted and calls
  // done once the GPU has run it. False when there is no fence, so the
  // caller frees the slot itself.
  bool fence(
      EMSCRIPTEN_WEBGL_CONTEXT_HANDLE contextGL,
      double submitted,
      std::function<void()> done)
  {
    if (m_fenceTimeouts >= kMaxFenceTimeouts)
    {
      return false;
    }
    MakeCurrent makeCurrent(contextGL);
    GLsync sync = makeCurrent.ok()
                      ? glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0)
                      : nullptr;
    if (sync == nullptr)
    {
      return false;
    }
    glFlush();
    m_frames.push_back(
        {contextGL, sync, submitted, emscripten_get_now(), std::move(done)});
    if (m_frames.size() == 1)
    {
      schedulePoll();
    }
    return true;
  }

  // Frees every slot now, for a context that is going away.
  void retireAll()
  {
    while (!m_frames.empty())
    {
      MakeCurrent makeCurrent(m_frames.front().contextGL);
      retireFront(makeCurrent.ok());
    }
  }

private:
  struct Frame
  {
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE contextGL;
    GLsync sync;
    double submitted;
    double fenced;
    std::function<void()> done;
  };

  class MakeCurrent
  {
  public:
    MakeCurrent(EMSCRIPTEN_WEBGL_CONTEXT_HANDLE contextGL)
        : m_previous(emscripten_webgl_get_current_context())
    {
      m_ok = contextGL != 0 && (contextGL == m_previous ||
                                emscripten_webgl_make_context_current(
                                    contextGL) == EMSCRIPTEN_RESULT_SUCCESS);
      m_restore = contextGL != m_previous && m_previous != 0;
    }
    // Failing on a destroyed context still unbinds the current one, so
    // restore it either way.
    ~MakeCurrent()
    {
      if (m_restore)
      {
        emscripten_webgl_make_context_current(m_previous);
      }
    }
    bool ok() const { return m_ok; }

  private:
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE m_previous;
    bool m_ok;
    bool m_restore;
  };

  void schedulePoll()
  {
    ref();
    emscripten_set_timeout(&Poll, 1, this);
  }

  // WebGL2 can only poll a fence, and only sees it signal between tasks.
  static void Poll(void* userData)
  {
    rcp<WebGPUFrames> self(static_cast<WebGPUFrames*>(userData));
    const double now = emscripten_get_now();
    while (!self->m_frames.empty())
    {
      const Frame& front = self->m_frames.front();
      MakeCurrent makeCurrent(front.contextGL);
      // A lost context fails the wait, which retires the frame too.
      if (makeCurrent.ok() &&
          glClientWaitSync(front.sync, 0, 0) == GL_TIMEOUT_EXPIRED)
      {
        // A fence that never signals would hold its slot for good. A
        // slow first frame trips this too, so only fences that keep
        // failing make slots free at replay instead.
        if (now - front.fenced > kFenceDeadlineMs)
        {
          if (++self->m_fenceTimeouts >= kMaxFenceTimeouts)
          {
            self->m_replayMs.store(0, std::memory_order_relaxed);
          }
          self->retireAll();
          return;
        }
        self->m_lastPoll = now;
        self->schedulePoll();
        return;
      }
      // The GPU ran frames back to back, so a frame's cost starts when
      // the one before it finished, if that was after it was submitted.
      const double start = std::max(front.submitted, self->m_lastDone);
      // Polls are coarse, so it finished somewhere since the last one,
      // and not before its fence.
      const double done = (std::max({self->m_lastPoll, start, front.fenced}) +
                           now) /
                          2;
      self->noteReplayMs(done - start);
      self->m_lastDone = done;
      self->retireFront(makeCurrent.ok());
    }
  }

  // With no context the sync died with it.
  void retireFront(bool contextOK)
  {
    Frame frame = std::move(m_frames.front());
    m_frames.pop_front();
    if (contextOK)
    {
      glDeleteSync(frame.sync);
    }
    frame.done();
  }

  // A trimmed mean, since polling quantizes each sample but evens out
  // across several, and one slow frame should not move it.
  void noteReplayMs(double ms)
  {
    m_samples[m_sampleCount++ % kSamples] = static_cast<float>(ms);
    std::array<float, kSamples> sorted = m_samples;
    const size_t count = std::min(m_sampleCount, kSamples);
    std::sort(sorted.begin(), sorted.begin() + count);
    const size_t trim = count / 4;
    float sum = 0;
    for (size_t i = trim; i < count - trim; ++i)
    {
      sum += sorted[i];
    }
    m_replayMs.store(sum / (count - 2 * trim), std::memory_order_relaxed);
  }

  static constexpr double kFenceDeadlineMs = 1000;
  static constexpr uint32_t kMaxFenceTimeouts = 3;
  static constexpr size_t kSamples = 9;

  std::deque<Frame> m_frames;
  std::atomic<float> m_replayMs{0};
  double m_lastDone = 0;
  double m_lastPoll = 0;
  uint32_t m_fenceTimeouts = 0;
  std::array<float, kSamples> m_samples{};
  size_t m_sampleCount = 0;
};

} // namespace rive::cmd

#endif
