/*
 * Copyright 2026 Rive
 */

#pragma once

#include "common/testing_window.hpp"
#include "gm/ore_gm_helper.hpp"
#include <catch.hpp>
#include <cstddef>
#include <cstdlib>
#ifdef __APPLE__
#include <dlfcn.h>
#endif

namespace ore_test
{
// Apple's GL is ANGLE since desktop GL caps at 4.1. Elsewhere it is GLFW,
// as our EGL path speaks only ANGLE, which Mesa and Android cannot serve.
constexpr TestingWindow::Backend kGLBackends[] = {
#if defined(__APPLE__)
    TestingWindow::Backend::angle,
#else
    TestingWindow::Backend::gl,
#endif
};

// `TestingWindow::Init` aborts on a backend it cannot bring up rather than
// declining it, which would take every test in the binary with it.
inline bool backendCanStart(TestingWindow::Backend backend)
{
  if (backend == TestingWindow::Backend::gl)
  {
    return getenv("DISPLAY") != nullptr || getenv("WAYLAND_DISPLAY") != nullptr;
  }
#ifdef __APPLE__
  if (backend == TestingWindow::Backend::angle)
  {
    // Held for the process; MakeEGL dlopens it again anyway.
    return dlopen("libEGL.dylib", RTLD_LAZY) != nullptr;
  }
#endif
  return true;
}

// Returns how many backends ran.
template <size_t N>
int runOnBackends(
    const TestingWindow::Backend (&backends)[N],
    void (*scenario)(TestingWindow*))
{
  int ran = 0;
  for (auto backend : backends)
  {
    if (!backendCanStart(backend))
      continue;
    auto* window = TestingWindow::Init(
        backend,
        {},
        TestingWindow::Visibility::headless);
    if (window == nullptr || window->renderContext() == nullptr ||
        !ore_gm::isOreBackendActive())
    {
      TestingWindow::Destroy();
      continue;
    }
    INFO("backend " << TestingWindow::BackendName(backend));
    scenario(window);
    ++ran;
    TestingWindow::Destroy();
  }
  return ran;
}
} // namespace ore_test
