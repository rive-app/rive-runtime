/*
 * Copyright 2022 Rive
 */

// Shared between goldens.cpp and the env gated diagnostics in
// goldens_bench.cpp.

#pragma once

// Don't compile the goldens tool as part of the "tests" project.
#ifndef TESTING

#include "goldens_arguments.hpp"
#include "common/test_harness.hpp"
#include "common/testing_window.hpp"
#include "rive/artboard.hpp"
#include "rive/renderer.hpp"
#include "rive/file.hpp"
#include "rive/refcnt.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/static_scene.hpp"
#ifdef WITH_RIVE_SCRIPTING
#include "rive/lua/scripting_vm.hpp"
#include "rive/lua/rive_lua_libs.hpp"
#endif
#if defined(WITH_RIVE_SCRIPTING) && defined(RIVE_CANVAS)
// RIVE_GOLDENS_DEFER_ORE records through a DeferredOreContext and replays on
// the real context in the same frame, single threaded.
#include "common/testing_window_sink.hpp"
#include "rive/renderer/render_context.hpp"
#include "rive/renderer/cmd/deferred_session.hpp"
#endif
#include <cstdlib>
#include <memory>
#include <vector>

// Builds without scripting or canvas never include the deferred headers but
// the accessors below still name the type.
namespace rive::cmd
{
class DeferredSession;
}

extern GoldensArguments s_args;

// Consoles build without an env API, probes just come back unset there.
inline const char* goldens_getenv(const char* name)
{
#ifdef NO_GETENV
    return nullptr;
#else
    return getenv(name);
#endif
}

void dumpPixelsAsPng(const char* rivName,
                     int windowWidth,
                     int windowHeight,
                     std::vector<uint8_t> pixels);

#if defined(WITH_RIVE_SCRIPTING) && defined(RIVE_CANVAS)
// Goldens frames present on a white background.
inline TestingWindowFrameSink goldensFrameSink(bool doClear = true)
{
    return TestingWindowFrameSink({
        .clearColor = 0xffffffff,
        .doClear = doClear,
        .triangulationThresholds = DeterministicTriangulationThresholds,
    });
}

// RIVE_GOLDENS_BENCH=<iters> loads the same scene immediate and deferred and
// reports per frame record cost, replay cost, and stream size.
void run_benchmark(const std::vector<uint8_t>& bytes,
                   const char* artboardName,
                   const char* stateMachineName,
                   int iters);
#endif

class RIVLoader
{
public:
    // Auto defers for --deferred, or RIVE_GOLDENS_DEFER_ORE with a single
    // cell. The benchmark forces one or the other to load both side by side.
    enum class DeferMode
    {
        Auto,
        Immediate,
        Deferred
    };

    RIVLoader(const std::vector<uint8_t>& rivBytes,
              const char* artboardName,
              const char* stateMachineName,
              DeferMode mode = DeferMode::Auto)
    {
        rive::Factory* importFactory = TestingWindow::Get()->factory();
#if defined(WITH_RIVE_SCRIPTING) && defined(RIVE_CANVAS)
        // Importing through the DeferredSession makes the artboard's own 2D
        // resources deferred objects with ids so drawInternal can record.
        bool wantDeferred =
            mode == DeferMode::Deferred ||
            (mode == DeferMode::Auto &&
             (s_args.deferred() || (goldens_getenv("RIVE_GOLDENS_DEFER_ORE") &&
                                    s_args.cols() * s_args.rows() == 1)));
        if (wantDeferred)
        {
            if (auto* rc = TestingWindow::Get()->renderContext())
            {
                if (auto* ore = rc->getOreContext())
                {
                    m_session = std::make_unique<rive::cmd::DeferredSession>(
                        rive::ore::ReplayCaps::from(*ore));
                    // Bound before import so registration scripts that reach
                    // for the device find it, like a real host.
                    m_session->bindRenderContext(rc);
                    importFactory = m_session.get();
                }
            }
        }
#endif
        m_file = rive::File::import(rivBytes, importFactory);
        if (m_file == nullptr)
        {
            throw "Bad riv file";
        }
        if (artboardName != nullptr && artboardName[0] != '\0')
        {
            m_artboard = m_file->artboardNamed(artboardName);
        }
        else
        {
            m_artboard = m_file->artboardDefault();
        }
        if (m_artboard == nullptr)
        {
            throw "Can't load artboard";
        }

        // Bind the default view model instance
        m_viewModelInstance = m_file->createViewModelInstance(m_artboard.get());
        m_artboard->bindViewModelInstance(m_viewModelInstance);

        if (stateMachineName != nullptr && stateMachineName[0] != '\0')
        {
            m_scene = m_artboard->stateMachineNamed(stateMachineName);
        }
        else
        {
            m_scene = m_artboard->defaultStateMachine();
        }

        if (m_scene == nullptr)
        {
            // This is a riv without any state machines. Just draw the artboard.
            m_scene = std::make_unique<rive::StaticScene>(m_artboard.get());
        }

        if (m_scene != nullptr && m_viewModelInstance != nullptr)
        {
            m_scene->bindViewModelInstance(m_viewModelInstance);
        }
    }

    rive::Scene* stateMachine() const { return m_scene.get(); }
    rive::Artboard* artboard() const { return m_artboard.get(); }

    // Null when deferred mode is off.
    rive::cmd::DeferredSession* deferredSession() const
    {
#if defined(WITH_RIVE_SCRIPTING) && defined(RIVE_CANVAS)
        return m_session.get();
#else
        return nullptr;
#endif
    }

private:
    // Destroyed last since deferred resources held by the file record their
    // destruction into the session, so it must outlive them.
#if defined(WITH_RIVE_SCRIPTING) && defined(RIVE_CANVAS)
    std::unique_ptr<rive::cmd::DeferredSession> m_session;
#endif
    rive::rcp<rive::File> m_file;
    std::unique_ptr<rive::ArtboardInstance> m_artboard;
    std::unique_ptr<rive::Scene> m_scene;
    rive::rcp<rive::ViewModelInstance> m_viewModelInstance;
};

#endif // TESTING
