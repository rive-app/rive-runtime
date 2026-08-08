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
#include "rive/renderer.hpp"
#include "rive/scene.hpp"
#include "rive/text/font_hb.hpp"
#include "rive/text/raw_text.hpp"
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
#include <fstream>

#if defined(RIVE_ANDROID) && !defined(RIVE_UNREAL)
#include "common/rive_android_app.hpp"
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

Player::Player() : m_fps(std::make_unique<FPSOverlay>()) {}

Player::~Player() = default;

void Player::shutdown()
{
#ifdef RIVE_CANVAS
    if (m_session != nullptr)
    {
        // Session made resources unwind through the session and the resident
        // tables against the live context, not at exit.
        m_fps.reset();
        m_scene = nullptr;
        m_artboard = nullptr;
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
                m_session = std::make_unique<rive::cmd::DeferredSession>(ore);
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
    }
#else
    if (m_useDeferred)
    {
        printf("--deferred requires a RIVE_CANVAS build, drawing immediate\n");
    }
#endif
    m_file = rive::File::import(rivBytes, m_factory);
    assert(m_file);

#ifdef WITH_RIVE_SCRIPTING
    // Wire contexts before artboard instantiation; verify hooks may
    // call context:gpuCanvas() during construction.
    if (auto* vm = m_file->scriptingVM())
    {
        if (auto* sctx = vm->context())
        {
            sctx->setRenderContext(TestingWindow::Get()->renderContext());
#ifdef RIVE_CANVAS
            if (m_session != nullptr)
            {
                sctx->setOreContext(&m_session->oreContext());
                // Regular canvas 2D content records into the deferred stream.
                sctx->setDeferredCanvasHost(m_session.get());
            }
#endif
        }
    }
#endif

    m_artboard = m_file->artboardDefault();
    assert(m_artboard);
    m_scene = m_artboard->defaultStateMachine();
    if (!m_scene)
    {
        m_scene = m_artboard->animationAt(0);
    }
    assert(m_scene);

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
    if (m_zoomLevel != 0)
    {
        float scale = powf(1.25f, m_zoomLevel);
        renderer->translate(width / 2.f, height / 2.f);
        renderer->scale(scale, scale);
        renderer->translate(width / -2.f, height / -2.f);
    }

    // Draw the .riv.
    renderer->save();
    renderer->align(rive::Fit::contain,
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
#ifdef RIVE_CANVAS
    if (m_session != nullptr)
    {
        // Snapshot replay is the same path a threaded consumer takes.
        rive::cmd::DeferredFrame frame = rive::cmd::snapshotFrame(*m_session);
        m_session->resetFrame();
        TestingWindowFrameSink sink(frameOptions);
        m_replayer->replayFrame(frame, sink);
        uint32_t dropped = m_replayer->droppedDraws();
        if (dropped != 0 && dropped != m_lastDroppedDraws)
        {
            printf("deferred replay dropped %u draws\n", dropped);
        }
        m_lastDroppedDraws = dropped;
    }
#endif
    TestingWindow::Get()->endFrame();

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
        computeAlignment(rive::Fit::contain,
                         rive::Alignment::center,
                         rive::AABB(0, 0, width, height),
                         m_artboard->bounds());

    // Consume all input events until none are left in the queue
    TestingWindow::InputEventData inputEventData;
    while (TestingWindow::Get()->consumeInputEvent(inputEventData))
    {
        const rive::Vec2D mousePosAligned =
            alignmentMat.invertOrIdentity() *
            rive::Vec2D(inputEventData.metadata.posX,
                        inputEventData.metadata.posY);

        switch (inputEventData.eventType)
        {
            case TestingWindow::InputEvent::KeyPress:
                keyPressed(inputEventData.metadata.key);
                break;

            case TestingWindow::InputEvent::MouseMove:
                m_scene->pointerMove(mousePosAligned);
                break;

            case TestingWindow::InputEvent::MouseDown:
                m_scene->pointerDown(mousePosAligned);
                break;

            case TestingWindow::InputEvent::MouseUp:
                m_scene->pointerUp(mousePosAligned);
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
