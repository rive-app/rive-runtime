/*
 * Copyright 2026 Rive
 */

#pragma once

#if defined(__EMSCRIPTEN_PTHREADS__)

#include <atomic>
#include <functional>
#include <utility>
#include <emscripten/proxying.h>
#include <emscripten/threading.h>
#include <pthread.h>

namespace rive::cmd
{

// Each web host installs its replay worker's message listener here. Runs once
// on the worker, before it hears any message.
void webReplayWorkerListen();

// The one web thread that owns every transferred canvas's GL context and
// replays what the page records for them. Leaky, it lives with the page.
class WebReplayWorker
{
public:
  static WebReplayWorker& Instance()
  {
    static WebReplayWorker* worker = new WebReplayWorker();
    return *worker;
  }

  pthread_t thread() const { return m_thread; }
  bool onWorker() const { return pthread_equal(pthread_self(), m_thread); }
  // Messages carrying canvases and images need the listener installed first.
  bool listening() const { return m_listening.load(std::memory_order_acquire); }

  // Runs once the worker returns to its event loop, which is also when its
  // canvases present.
  void post(std::function<void()>&& job)
  {
    m_queue.proxyAsync(m_thread, std::move(job));
  }
  void postToMain(std::function<void()>&& job)
  {
    m_queue.proxyAsync(emscripten_main_runtime_thread_id(), std::move(job));
  }

private:
  WebReplayWorker()
  {
    pthread_create(&m_thread, nullptr, &WebReplayWorker::Main, this);
  }

  static void* Main(void* arg)
  {
    auto* self = static_cast<WebReplayWorker*>(arg);
    emscripten_set_thread_name(pthread_self(), "rive.deferred.replay");
    webReplayWorkerListen();
    self->m_listening.store(true, std::memory_order_release);
    emscripten_exit_with_live_runtime();
    return nullptr;
  }

  emscripten::ProxyingQueue m_queue;
  pthread_t m_thread{};
  std::atomic<bool> m_listening{false};
};

} // namespace rive::cmd

#endif
