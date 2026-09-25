/*
 * Copyright 2024 Rive
 */

// Don't compile this file as part of the "tests" project.
#ifndef TESTING

#include "player.hpp"

#include <sstream>
#include "common/test_harness.hpp"
#include "common/testing_window.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/artboard.hpp"
#include "rive/file.hpp"
#include "rive/input/gamepad_batch.hpp"
#include "rive/renderer.hpp"
#include "rive/renderer/scoped_autorelease_pool.hpp"
#include "rive/scene.hpp"
#include "rive/text/font_hb.hpp"
#include "rive/text/raw_text.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#ifdef WITH_RIVE_SCRIPTING
#include "rive/lua/rive_lua_libs.hpp"
#include "rive/lua/scripting_vm.hpp"
#include "rive/renderer/render_context.hpp"
#endif
#ifdef RIVE_CANVAS
#include "common/testing_window_sink.hpp"
#include "rive/renderer/cmd/deferred_session.hpp"
#endif
#include "assets/roboto_flex.ttf.hpp"
#include <stdio.h>
#include <condition_variable>
#include <fstream>
#include <mutex>
#include <optional>
#include <thread>

#if defined(RIVE_ANDROID) && !defined(RIVE_UNREAL)
#include "common/rive_android_app.hpp"
#endif

#if (defined(RIVE_IOS) || defined(RIVE_IOS_SIMULATOR)) && !defined(RIVE_UNREAL)
#include "common/rive_ios_app.hpp"
#endif

#ifdef __EMSCRIPTEN__
#include "common/rive_wasm_app.hpp"
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#endif

// The FPS readout drawn over the .riv, and the resources it needs.
struct Player::FPSOverlay
{
    rive::rcp<rive::Font> roboto;
    rive::rcp<rive::RenderPaint> blackStroke;
    rive::rcp<rive::RenderPaint> whiteFill;
    std::unique_ptr<rive::RawText> text;
    int frames = 0;
    std::chrono::high_resolution_clock::time_point timeLastUpdate;
};

struct Player::FrameJob
{
    TestingWindow::FrameOptions options;
#ifdef RIVE_CANVAS
    rive::cmd::DeferredFrame frame;
#endif
    std::vector<uint8_t>* pixels = nullptr;
};

// One frame in flight at most: submit blocks until the previous frame has
// presented. Every window call happens on this thread while it lives.
struct Player::RenderThread
{
    explicit RenderThread(Player* player) :
        m_thread([this, player] { run(player); })
    {}

    ~RenderThread()
    {
        {
            std::lock_guard lock(m_mutex);
            m_quit = true;
        }
        m_cv.notify_all();
        m_thread.join();
    }

    void submit(FrameJob job)
    {
        std::unique_lock lock(m_mutex);
        m_cv.wait(lock, [this] { return idle(); });
        m_pending = std::move(job);
        m_cv.notify_all();
    }

    // Returns once every submitted frame has presented.
    void drain()
    {
        std::unique_lock lock(m_mutex);
        m_cv.wait(lock, [this] { return idle(); });
    }

private:
    bool idle() const { return !m_pending.has_value(); }

    void run(Player* player)
    {
        for (;;)
        {
            std::unique_lock lock(m_mutex);
            m_cv.wait(lock, [this] { return m_pending.has_value() || m_quit; });
            if (!m_pending.has_value())
            {
                return;
            }
            lock.unlock();
            {
                rive::gpu::ScopedAutoreleasePool autoreleasePool;
                player->presentFrame(*m_pending);
            }
            lock.lock();
            m_pending.reset();
            lock.unlock();
            m_cv.notify_all();
        }
    }

    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::optional<FrameJob> m_pending;
    bool m_quit = false;
    std::thread m_thread; // last, so the state above exists before it runs
};

Player::Player() : m_fps(std::make_unique<FPSOverlay>()) {}

// Joins before the replay state below goes, for hosts that skip shutdown.
Player::~Player() { m_renderThread = nullptr; }

void Player::shutdown()
{
    m_renderThread = nullptr;
#ifdef RIVE_CANVAS
    if (m_session != nullptr)
    {
        // Session made resources unwind through the session and the resident
        // tables against the live context, not at exit.
        m_fps.reset();
        m_scene = nullptr;
        m_artboard = nullptr;
        m_viewModelInstance = nullptr;
        m_file = nullptr;
        m_session = nullptr;
        m_replayer = nullptr;
    }
#endif
}

static void update_parameter(int& val, int multiplier, char key, bool seenBang)
{
    if (seenBang)
        val = multiplier;
    else if (key >= 'a')
        val += multiplier;
    else
        val -= multiplier;
}

void Player::keyPressed(char key)
{
    if (key >= '0' && key <= '9')
    {
        m_keyMultiplier = m_keyMultiplier * 10 + (key - '0');
        m_seenDigit = true;
        return;
    }
    if (key == '!')
    {
        m_seenBang = true;
        return;
    }
    if (!m_seenDigit)
    {
        m_keyMultiplier = m_seenBang ? 0 : 1;
    }
    switch (key)
    {
        case 'h':
        case 'H':
            update_parameter(m_copiesLeft, m_keyMultiplier, key, m_seenBang);
            break;
        case 'k':
        case 'K':
            update_parameter(m_copiesAbove, m_keyMultiplier, key, m_seenBang);
            break;
        case 'l':
        case 'L':
            update_parameter(m_copiesRight, m_keyMultiplier, key, m_seenBang);
            break;
        case 'j':
        case 'J':
            update_parameter(m_copiesBelow, m_keyMultiplier, key, m_seenBang);
            break;
        case 'x':
        case 'X':
            update_parameter(m_copiesLeft, m_keyMultiplier, key, m_seenBang);
            update_parameter(m_copiesRight, m_keyMultiplier, key, m_seenBang);
            break;
        case 'y':
        case 'Y':
            update_parameter(m_copiesAbove, m_keyMultiplier, key, m_seenBang);
            update_parameter(m_copiesBelow, m_keyMultiplier, key, m_seenBang);
            break;
        case 'r':
        case 'R':
            update_parameter(m_rotations90, m_keyMultiplier, key, m_seenBang);
            break;
        case 'z':
        case 'Z':
            update_parameter(m_zoomLevel, m_keyMultiplier, key, m_seenBang);
            break;
        case 's':
        case 'S':
            update_parameter(m_spacing, m_keyMultiplier, key, m_seenBang);
            break;
        case 'm':
            m_monitorIdx += m_keyMultiplier;
            break;
        case 'p':
            m_paintStyle = (m_paintStyle + m_keyMultiplier) % 3;
            break;
        case 'P':
            m_paintStyle = (m_paintStyle + 3 - (m_keyMultiplier % 3)) % 3;
            break;
        case 'w':
            m_wireframe = !m_wireframe;
            break;
        case 'u':
            m_paused = !m_paused;
            break;
        case 'f':
            m_forceFixedDeltaTime = !m_forceFixedDeltaTime;
            break;
        case 'q':
        case '\x03': // ^C
            m_quit = true;
            break;
        case '\x1b': // Esc
            break;
        case '`':
            m_hotloadShaders = true;
            break;
        case '~':
            if (m_inputMode == InputMode::chars)
            {
                m_inputMode = InputMode::consolecommands;
            }
            else
            {
                m_inputMode = InputMode::chars;
            }
            break;
        default:
            // fprintf(stderr, "invalid option: %c\n", key);
            // abort();
            break;
    }
    m_keyMultiplier = 0;
    m_seenDigit = false;
    m_seenBang = false;
}

bool Player::parseArgs(int argc,
                       const char* const argv[],
                       FrameRunner::LaunchOptions& options)
{
    bool onlyUbershaders = false;

    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--test_harness") == 0)
        {
            TestHarness::Instance().init(TCPClient::Connect(argv[++i]), 0);
            if (!TestHarness::Instance().fetchRivFile(m_rivName,
                                                      m_pendingRivBytes))
            {
                fprintf(stderr, "failed to fetch a riv file.");
                abort();
            }
        }
        else if (strcmp(argv[i], "--backend") == 0 ||
                 strcmp(argv[i], "-b") == 0)
        {
            options.backend =
                TestingWindow::ParseBackend(argv[++i], &options.backendParams);
        }
        else if (argv[i][0] == '-' &&
                 argv[i][1] == 'b') // "-bvk" without a space.
        {
            options.backend =
                TestingWindow::ParseBackend(argv[i] + 2,
                                            &options.backendParams);
        }
        else if (strcmp(argv[i], "--options") == 0 ||
                 strcmp(argv[i], "-k") == 0)
        {
            for (const char* k = argv[++i]; *k; ++k)
            {
                keyPressed(*k);
            }
        }
        else if (strcmp(argv[i], "--only_ubershaders") == 0 ||
                 strcmp(argv[i], "-u") == 0)
        {
            onlyUbershaders = true;
        }
        else if (strcmp(argv[i], "--deferred") == 0)
        {
            m_useDeferred = true;
        }
        else if (strcmp(argv[i], "--fit") == 0 && i + 1 < argc)
        {
            const char* fit = argv[++i];
            m_fitSet = true;
            m_fit = strcmp(fit, "layout") == 0  ? rive::Fit::layout
                    : strcmp(fit, "cover") == 0 ? rive::Fit::cover
                    : strcmp(fit, "fill") == 0  ? rive::Fit::fill
                                                : rive::Fit::contain;
        }
        else if (strcmp(argv[i], "--threaded") == 0)
        {
            m_threaded = true;
        }
        else if (strcmp(argv[i], "--dump") == 0 && i + 1 < argc)
        {
            m_dumpPath = argv[++i];
        }
        else if (strcmp(argv[i], "--dump-frame") == 0 && i + 1 < argc)
        {
            m_dumpFrame = atoi(argv[++i]);
        }
        else if (argv[i][0] == '-' &&
                 argv[i][1] == 'k') // "-k1234asdf" without a space.
        {
            for (const char* k = argv[i] + 2; *k; ++k)
            {
                keyPressed(*k);
            }
        }
        else if (strcmp(argv[i], "--window") == 0 || strcmp(argv[i], "-w") == 0)
        {
            options.visibility = TestingWindow::Visibility::window;
        }
        else
        {
            // No argument name defaults to the source riv.
            if (strcmp(argv[i], "--src") == 0 || strcmp(argv[i], "-s") == 0)
            {
                ++i;
            }
            m_rivName = argv[i];
            std::ifstream rivStream(m_rivName, std::ios::binary);
            m_pendingRivBytes =
                std::vector<uint8_t>(std::istreambuf_iterator<char>(rivStream),
                                     {});
        }
    }

    if (onlyUbershaders)
    {
        options.backendParams.shaderCompilationMode =
            rive::gpu::ShaderCompilationMode::onlyUbershaders;
    }

    return !m_pendingRivBytes.empty();
}

void Player::init()
{
    init(std::move(m_rivName), std::move(m_pendingRivBytes));
}

void Player::init(std::string rivName, std::vector<uint8_t> rivBytes)
{
    m_rivName = std::move(rivName);
    m_factory = TestingWindow::Get()->factory();
#ifdef RIVE_CANVAS
    // Importing through the DeferredSession makes the artboard's own 2D
    // resources deferred objects with ids so drawInternal can record.
    if (m_useDeferred)
    {
        if (auto* rc = TestingWindow::Get()->renderContext())
        {
            if (auto* ore = rc->getOreContext())
            {
                m_session = std::make_unique<rive::cmd::DeferredSession>(
                    rive::ore::ReplayCaps::from(*ore));
                // Bound before import so registration scripts that reach for
                // the device find it, like a real host.
                m_session->bindRenderContext(rc);
                m_replayer = std::make_unique<rive::cmd::DeferredReplayer>();
                m_factory = m_session.get();
            }
        }
        if (m_session == nullptr)
        {
            printf("--deferred unavailable on this backend, drawing "
                   "immediate\n");
        }
        else if (m_threaded && TestingWindow::Get()->supportsRenderThread())
        {
            m_renderThread = std::make_unique<RenderThread>(this);
        }
    }
#else
    if (m_useDeferred)
    {
        printf("--deferred requires a RIVE_CANVAS build, drawing immediate\n");
    }
#endif
    if (m_threaded && m_renderThread == nullptr)
    {
        printf("--threaded needs deferred replay on a window that presents "
               "off the main thread, presenting inline\n");
    }
    m_file = rive::File::import(rivBytes, m_factory);
    assert(m_file);

    m_artboard = m_file->artboardDefault();
    assert(m_artboard);
    // A layout artboard is authored to resize, so it fills the window like
    // our other hosts rather than letterboxing at its design size.
    if (!m_fitSet && m_artboard->style() != nullptr)
    {
        m_fit = rive::Fit::layout;
    }

    // Bind the artboard's default view model instance, if it has one, the same
    // way a real host runtime would.
    m_viewModelInstance =
        m_file->createDefaultViewModelInstance(m_artboard.get());
    if (m_viewModelInstance != nullptr)
    {
        m_artboard->bindViewModelInstance(m_viewModelInstance);
    }

    auto stateMachine = m_artboard->defaultStateMachine();
    if (!stateMachine && m_artboard->stateMachineCount() > 0)
    {
        stateMachine = m_artboard->stateMachineAt(0);
    }
    m_stateMachine = stateMachine.get();
    m_scene = std::move(stateMachine);
    if (!m_scene)
    {
        m_scene = m_artboard->animationAt(0);
    }
    assert(m_scene);
    if (m_viewModelInstance != nullptr)
    {
        m_scene->bindViewModelInstance(m_viewModelInstance);
    }

    // Setup FPS.
    m_fps->roboto = HBFont::Decode(assets::roboto_flex_ttf());
    m_fps->blackStroke = m_factory->makeRenderPaint();
    m_fps->blackStroke->color(0xff000000);
    m_fps->blackStroke->style(rive::RenderPaintStyle::stroke);
    m_fps->blackStroke->thickness(4);
    m_fps->whiteFill = m_factory->makeRenderPaint();
    m_fps->whiteFill->color(0xffffffff);
    m_fps->timeLastUpdate = std::chrono::high_resolution_clock::now();
    m_timestampPrevFrame = std::chrono::high_resolution_clock::now();
}

// The state machine takes gamepads as the wire batch our js embedder sends.
static void appendU32(std::vector<uint8_t>& buffer, uint32_t value)
{
    for (int i = 0; i < 4; ++i)
    {
        buffer.push_back(static_cast<uint8_t>(value >> (8 * i)));
    }
}

void Player::submitGamepad(const TestingWindow::InputEventData& event)
{
    if (m_stateMachine == nullptr)
    {
        return;
    }
    const auto& pad = event.metadata.pad;
    std::vector<uint8_t> buffer;
    appendU32(buffer, rive::kGamepadBatchWireVersion);
    if (event.eventType == TestingWindow::InputEvent::GamepadConnected)
    {
        buffer.push_back(
            static_cast<uint8_t>(rive::GamepadRecordType::connected));
        appendU32(buffer, pad.deviceId);
        buffer.push_back(0); // standard mapping
        buffer.push_back(pad.buttonCount);
        buffer.push_back(pad.axisCount);
        buffer.push_back(0); // padding
        for (int i = 0; i < pad.buttonCount + pad.axisCount; ++i)
        {
            appendU32(buffer, 0); // every value at rest
        }
    }
    else if (event.eventType == TestingWindow::InputEvent::GamepadDisconnected)
    {
        buffer.push_back(
            static_cast<uint8_t>(rive::GamepadRecordType::disconnected));
        appendU32(buffer, pad.deviceId);
    }
    else
    {
        buffer.push_back(static_cast<uint8_t>(rive::GamepadRecordType::update));
        appendU32(buffer, pad.deviceId);
        buffer.push_back(1); // one change
        buffer.push_back(pad.isAxis ? 1 : 0);
        buffer.push_back(pad.index);
        uint32_t bits;
        memcpy(&bits, &pad.value, sizeof(bits));
        appendU32(buffer, bits);
    }
    m_stateMachine->submitGamepadsFromBuffer(buffer.data(), buffer.size());
}

void Player::presentFrame(FrameJob& job)
{
#ifdef RIVE_CANVAS
    if (m_session != nullptr)
    {
        TestingWindowFrameSink sink(job.options);
        m_replayer->replayFrame(job.frame, sink);
        uint32_t dropped = m_replayer->droppedDraws();
        if (dropped != 0 && dropped != m_lastDroppedDraws)
        {
            printf("deferred replay dropped %u draws\n", dropped);
        }
        m_lastDroppedDraws = dropped;
    }
#endif
    TestingWindow::Get()->endFrame(job.pixels);
}

bool Player::doFrame()
{
    if (m_quit || TestingWindow::Get()->shouldQuit()
#if defined(RIVE_ANDROID) && !defined(RIVE_UNREAL)
        || !rive_android_app_poll_once()
#endif
    )
    {
        m_quit = true;
        return false;
    }

#ifdef __EMSCRIPTEN__
    {
        // Fit the canvas to the browser window size.
        int windowWidth = EM_ASM_INT(return window["innerWidth"]);
        int windowHeight = EM_ASM_INT(return window["innerHeight"]);
        double devicePixelRatio = emscripten_get_device_pixel_ratio();
        int canvasExpectedWidth = windowWidth * devicePixelRatio;
        int canvasExpectedHeight = windowHeight * devicePixelRatio;
        if (TestingWindow::Get()->width() != canvasExpectedWidth ||
            TestingWindow::Get()->height() != canvasExpectedHeight)
        {
            printf("Resizing HTML canvas to %i x %i.\n",
                   canvasExpectedWidth,
                   canvasExpectedHeight);
            if (m_renderThread != nullptr)
            {
                m_renderThread->drain();
            }
            TestingWindow::Get()->resize(canvasExpectedWidth,
                                         canvasExpectedHeight);
            emscripten_set_element_css_size("#canvas",
                                            windowWidth,
                                            windowHeight);
        }
    }
#endif

    std::chrono::time_point timeNow = std::chrono::high_resolution_clock::now();
    const double elapsedS =
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            timeNow - m_timestampPrevFrame)
            .count() /
        1e9; // convert to s
    m_timestampPrevFrame = timeNow;

    float advanceDeltaTime = static_cast<float>(elapsedS);
    if (m_forceFixedDeltaTime)
    {
        advanceDeltaTime = 1.0f / 120;
    }

    // advanceAndApply drives the artboard through advanceInternal, which
    // skips the async-work pump in Artboard::advance; pump it here so scripted
    // image decodes resolve (without threads they only run when polled).
    m_artboard->pollAsyncWork();
#ifdef WITH_RIVE_SCRIPTING_WASM
    // Scavenges the script nursery, which otherwise grows until it traps.
    if (const char* warning = m_file->frameBoundary())
    {
        fprintf(stderr, "%s\n", warning);
    }
#endif
    m_scene->advanceAndApply(m_paused ? 0 : advanceDeltaTime);

    m_copiesLeft = std::max(m_copiesLeft, 0);
    m_copiesAbove = std::max(m_copiesAbove, 0);
    m_copiesRight = std::max(m_copiesRight, 0);
    m_copiesBelow = std::max(m_copiesBelow, 0);
    int copyCount = (m_copiesLeft + 1 + m_copiesRight) *
                    (m_copiesAbove + 1 + m_copiesBelow);
    if (copyCount != m_lastReportedCopyCount ||
        m_paused != m_lastReportedPauseState)
    {
        printf("Drawing %i copies of %s%s at %u x %u\n",
               copyCount,
               m_rivName.c_str(),
               m_paused ? " (paused)" : "",
               TestingWindow::Get()->width(),
               TestingWindow::Get()->height());
        m_lastReportedCopyCount = copyCount;
        m_lastReportedPauseState = m_paused;
    }

    const TestingWindow::FrameOptions frameOptions = {
        .clearColor = 0xff303030,
        .doClear = true,
        .wireframe = m_wireframe,
        .fillsDisabled = m_paintStyle == 2,
        .strokesDisabled = m_paintStyle == 1,
    };
    std::unique_ptr<rive::Renderer> renderer;
#ifdef RIVE_CANVAS
    if (m_session != nullptr)
    {
        m_session->recordOreReplayMarker();
        renderer = m_session->makeScreenRenderer();
    }
    else
#endif
    {
        renderer = TestingWindow::Get()->beginFrame(frameOptions);
    }

    if (m_hotloadShaders)
    {
        m_hotloadShaders = false;
#ifndef RIVE_NO_STD_SYSTEM
        std::system("sh rebuild_shaders.sh /tmp/rive");
        if (m_renderThread != nullptr)
        {
            m_renderThread->drain();
        }
        TestingWindow::Get()->hotloadShaders();
#endif
    }

    renderer->save();

    uint32_t width = TestingWindow::Get()->width();
    uint32_t height = TestingWindow::Get()->height();
    for (int i = m_rotations90; (i & 3) != 0; --i)
    {
        renderer->transform(rive::Mat2D(0, 1, -1, 0, width, 0));
        std::swap(height, width);
    }
    if (m_fit == rive::Fit::layout &&
        (m_artboard->width() != width || m_artboard->height() != height))
    {
        m_artboard->width(width);
        m_artboard->height(height);
        m_artboard->advance(0.0f);
    }
    if (m_zoomLevel != 0)
    {
        float scale = powf(1.25f, m_zoomLevel);
        renderer->translate(width / 2.f, height / 2.f);
        renderer->scale(scale, scale);
        renderer->translate(width / -2.f, height / -2.f);
    }

    // Draw the .riv.
    renderer->save();
    renderer->align(m_fit,
                    rive::Alignment::center,
                    rive::AABB(0, 0, width, height),
                    m_artboard->bounds());
    float spacingPx = m_spacing * 5 + 150;
    renderer->translate(-spacingPx * m_copiesLeft, -spacingPx * m_copiesAbove);
    for (int y = -m_copiesAbove; y <= m_copiesBelow; ++y)
    {
        renderer->save();
        for (int x = -m_copiesLeft; x <= m_copiesRight; ++x)
        {
            m_artboard->drawInternal(renderer.get());
            renderer->translate(spacingPx, 0);
        }
        renderer->restore();
        renderer->translate(0, spacingPx);
    }
    renderer->restore();

    if (m_fps->text != nullptr)
    {
        // Draw FPS.
        renderer->save();
        renderer->translate(0, 20);
        m_fps->text->render(renderer.get(), m_fps->blackStroke);
        m_fps->text->render(renderer.get(), m_fps->whiteFill);
        renderer->restore();
    }

    renderer->restore();

    std::vector<uint8_t> pixels;
    const bool dump = !m_dumpPath.empty() && ++m_frameCounter >= m_dumpFrame;
    FrameJob job = {.options = frameOptions,
                    .pixels = dump ? &pixels : nullptr};
#ifdef RIVE_CANVAS
    if (m_session != nullptr)
    {
        // Snapshot replay is the same path a threaded consumer takes.
        job.frame = rive::cmd::snapshotFrame(*m_session);
        m_session->resetFrame();
    }
#endif
    if (m_renderThread != nullptr)
    {
        m_renderThread->submit(std::move(job));
        if (dump)
        {
            m_renderThread->drain();
        }
    }
    else
    {
        presentFrame(job);
    }
    if (dump)
    {
        uint32_t w = TestingWindow::Get()->width();
        uint32_t h = TestingWindow::Get()->height();
        if (FILE* f = fopen(m_dumpPath.c_str(), "wb"))
        {
            fwrite(&w, sizeof(w), 1, f);
            fwrite(&h, sizeof(h), 1, f);
            fwrite(pixels.data(), 1, pixels.size(), f);
            fclose(f);
            printf("dumped %ux%u frame (%zu bytes) to %s\n",
                   w,
                   h,
                   pixels.size(),
                   m_dumpPath.c_str());
        }
        else
        {
            fprintf(stderr,
                    "could not open dump path %s\n",
                    m_dumpPath.c_str());
        }
        m_quit = true;
        return false;
    }

    // Count FPS.
    ++m_fps->frames;
    const double elapsedFPSUpdate =
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            timeNow - m_fps->timeLastUpdate)
            .count() /
        1e9; // convert to s
    if (elapsedFPSUpdate >= 2.0)
    {
        double fps = m_fps->frames / elapsedFPSUpdate;
        if (m_inputMode == InputMode::chars)
        {
            printf("[%.3f FPS]\n", fps);
        }

        char fpsRawText[32];
        snprintf(fpsRawText, sizeof(fpsRawText), "   %.1f FPS   ", fps);
        m_fps->text = std::make_unique<rive::RawText>(m_factory);
        m_fps->text->maxWidth(width);
#ifdef RIVE_ANDROID
        m_fps->text->align(rive::TextAlign::center);
#else
        m_fps->text->align(rive::TextAlign::right);
#endif
        m_fps->text->sizing(rive::TextSizing::fixed);
        m_fps->text->append(fpsRawText, nullptr, m_fps->roboto, 50.f);

        m_fps->frames = 0;
        m_fps->timeLastUpdate = timeNow;
    }

    const rive::Mat2D alignmentMat =
        computeAlignment(m_fit,
                         rive::Alignment::center,
                         rive::AABB(0, 0, width, height),
                         m_artboard->bounds());

    // Consume all input events until none are left in the queue
    TestingWindow::InputEventData inputEventData;
    while (TestingWindow::Get()->consumeInputEvent(inputEventData))
    {
        // Only pointer events carry a position in the union.
        auto alignedPos = [&]() {
            return alignmentMat.invertOrIdentity() *
                   rive::Vec2D(inputEventData.metadata.posX,
                               inputEventData.metadata.posY);
        };
        switch (inputEventData.eventType)
        {
            case TestingWindow::InputEvent::KeyPress:
                keyPressed(inputEventData.metadata.key);
                break;

            case TestingWindow::InputEvent::MouseMove:
                m_scene->pointerMove(alignedPos());
                break;

            case TestingWindow::InputEvent::MouseDown:
                m_scene->pointerDown(alignedPos());
                break;

            case TestingWindow::InputEvent::MouseUp:
                m_scene->pointerUp(alignedPos());
                break;
            case TestingWindow::InputEvent::GamepadConnected:
            case TestingWindow::InputEvent::GamepadDisconnected:
            case TestingWindow::InputEvent::GamepadChange:
                submitGamepad(inputEventData);
                break;
        }
    }

    std::string command;
    char key;

    if (m_inputMode == InputMode::consolecommands)
    {
        while (TestHarness::Instance().peekChar(key))
        {
            command += key;
        }

        std::istringstream iss(command);
        std::string first, second;
        iss >> first >> second;
        if (first == "~")
        {
            m_inputMode = InputMode::chars;
        }
        else if (first == "fire")
        {
            if (!second.empty())
            {
                if (auto* trigger = m_scene->getTrigger(second.c_str()))
                {
                    trigger->fire();
                }
            }
        }
    }
    else
    {
        while (TestHarness::Instance().peekChar(key))
        {
            keyPressed(key);
        }
    }

    return true;
}

// Unreal owns the main loop: it creates its own Player, parses the same
// arguments through Player::parseArgs(), and pumps doFrame() once per engine
// tick. Everything below (the global player, the shutdown, and main() itself)
// is only for the standalone tool.
#ifndef RIVE_UNREAL

static Player player;

static void player_shutdown()
{
    printf("\nShutting down\n");
    player.shutdown();
    TestingWindow::Destroy(); // Exercise our PLS teardown process now
                              // that we're done.
    TestHarness::Instance().shutdown();
#ifdef __EMSCRIPTEN__
    emscripten_cancel_main_loop();
    EM_ASM(if (window && window.close) window.close(););
#else
    exit(0);
#endif
}

#if defined(RIVE_IOS) || defined(RIVE_IOS_SIMULATOR)
int player_ios_main(int argc, const char* argv[])
#elif defined(RIVE_ANDROID)
int rive_android_main(int argc, const char* const* argv)
#elif defined(__EMSCRIPTEN__)
int rive_wasm_main(int argc, const char* const* argv)
#elif defined(EXTERN_TOOLS)
int rive_main(int argc, const char* argv[])
#else
int main(int argc, const char* argv[])
#endif
{
#ifdef _WIN32
    // Cause stdout and stderr to print immediately without buffering.
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
#endif

    FrameRunner::LaunchOptions options;
    const bool haveRiv = player.parseArgs(argc, argv, options);

    TestingWindow::Init(options.backend,
                        options.backendParams,
                        options.visibility,
#ifdef RIVE_ANDROID
                        rive_android_app_wait_for_window()
#elif defined(RIVE_IOS) || defined(RIVE_IOS_SIMULATOR)
                        rive_ios_app_wait_for_window()
#else
                        reinterpret_cast<void*>(
                            static_cast<intptr_t>(player.monitorIdx()))
#endif
    );

    if (!haveRiv)
    {
        fprintf(stderr, "no .riv file specified");
        abort();
    }

    player.init();

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(
        []() {
            if (!player.doFrame())
            {
                player_shutdown();
            }
        },
        0,
        true);
#else
    for (;;)
    {
        rive::gpu::ScopedAutoreleasePool autoreleasePool;
        if (!player.doFrame())
        {
            player_shutdown();
        }
    }
#endif

    return 0;
}

#endif // !RIVE_UNREAL

#endif
