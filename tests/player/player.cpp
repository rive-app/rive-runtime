/*
 * Copyright 2024 Rive
 */

// Don't compile this file as part of the "tests" project.
#ifndef TESTING

#include "common/test_harness.hpp"
#include "common/testing_window.hpp"
#include "rive/artboard.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/file.hpp"
#include "rive/text/font_hb.hpp"
#include "rive/text/raw_text.hpp"
#include "assets/roboto_flex.ttf.hpp"
#include <stdio.h>
#include <fstream>

#ifdef RIVE_ANDROID
#include "common/rive_android_app.hpp"
#endif

#ifdef __EMSCRIPTEN__
#include "common/rive_wasm_app.hpp"
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#endif

static void update_parameter(int& val, int multiplier, char key, bool seenBang)
{
    if (seenBang)
        val = multiplier;
    else if (key >= 'a')
        val += multiplier;
    else
        val -= multiplier;
}

static int copiesLeft = 0;
static int copiesAbove = 0;
static int copiesRight = 0;
static int copiesBelow = 0;
static int rotations90 = 0;
static int zoomLevel = 0;
static int spacing = 0;
static int monitorIdx = 0;
static bool wireframe = false;
static bool paused = false;
static bool forceFixedDeltaTime = false;
static bool quit = false;
static bool hotloadShaders = false;
static void key_pressed(char key)
{
    static int multiplier = 0;
    static bool seenDigit = false;
    static bool seenBang = false;
    if (key >= '0' && key <= '9')
    {
        multiplier = multiplier * 10 + (key - '0');
        seenDigit = true;
        return;
    }
    if (key == '!')
    {
        seenBang = true;
        return;
    }
    if (!seenDigit)
    {
        multiplier = seenBang ? 0 : 1;
    }
    switch (key)
    {
        case 'h':
        case 'H':
            update_parameter(copiesLeft, multiplier, key, seenBang);
            break;
        case 'k':
        case 'K':
            update_parameter(copiesAbove, multiplier, key, seenBang);
            break;
        case 'l':
        case 'L':
            update_parameter(copiesRight, multiplier, key, seenBang);
            break;
        case 'j':
        case 'J':
            update_parameter(copiesBelow, multiplier, key, seenBang);
            break;
        case 'x':
        case 'X':
            update_parameter(copiesLeft, multiplier, key, seenBang);
            update_parameter(copiesRight, multiplier, key, seenBang);
            break;
        case 'y':
        case 'Y':
            update_parameter(copiesAbove, multiplier, key, seenBang);
            update_parameter(copiesBelow, multiplier, key, seenBang);
            break;
        case 'r':
        case 'R':
            update_parameter(rotations90, multiplier, key, seenBang);
            break;
        case 'z':
        case 'Z':
            update_parameter(zoomLevel, multiplier, key, seenBang);
            break;
        case 's':
        case 'S':
            update_parameter(spacing, multiplier, key, seenBang);
            break;
        case 'm':
            monitorIdx += multiplier;
            break;
        case 'w':
            wireframe = !wireframe;
            break;
        case 'p':
            paused = !paused;
            break;
        case 'f':
            forceFixedDeltaTime = !forceFixedDeltaTime;
            break;
        case 'q':
        case '\x03': // ^C
            quit = true;
            break;
        case '\x1b': // Esc
            break;
        case '`':
            hotloadShaders = true;
            break;
        default:
            // fprintf(stderr, "invalid option: %c\n", key);
            // abort();
            break;
    }
    multiplier = 0;
    seenDigit = false;
    seenBang = false;
}

class Player
{
public:
    void init(std::string rivName, std::vector<uint8_t> rivBytes)
    {
        m_rivName = std::move(rivName);
        m_file = rive::File::import(rivBytes, TestingWindow::Get()->factory());
        assert(m_file);
        m_artboard = m_file->artboardDefault();
        assert(m_artboard);
        m_scene = m_artboard->defaultStateMachine();
        if (!m_scene)
        {
            m_scene = m_artboard->animationAt(0);
        }
        assert(m_scene);

        // Setup FPS.
        m_roboto = HBFont::Decode(assets::roboto_flex_ttf());
        m_blackStroke = TestingWindow::Get()->factory()->makeRenderPaint();
        m_blackStroke->color(0xff000000);
        m_blackStroke->style(rive::RenderPaintStyle::stroke);
        m_blackStroke->thickness(4);
        m_whiteFill = TestingWindow::Get()->factory()->makeRenderPaint();
        m_whiteFill->color(0xffffffff);
        m_timeLastFPSUpdate = std::chrono::high_resolution_clock::now();
        m_timestampPrevFrame = std::chrono::high_resolution_clock::now();
    }

    void doFrame()
    {
        if (quit || TestingWindow::Get()->shouldQuit()
#ifdef RIVE_ANDROID
            || !rive_android_app_poll_once()
#endif
        )
        {
            printf("\nShutting down\n");
            TestingWindow::Destroy(); // Exercise our PLS teardown process now
                                      // that we're done.
            TestHarness::Instance().shutdown();
#ifdef __EMSCRIPTEN__
            emscripten_cancel_main_loop();
            EM_ASM(window.close(););
#else
            exit(0);
#endif
            return;
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

        std::chrono::time_point timeNow =
            std::chrono::high_resolution_clock::now();
        const double elapsedS =
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                timeNow - m_timestampPrevFrame)
                .count() /
            1e9; // convert to s
        m_timestampPrevFrame = timeNow;

        float advanceDeltaTime = static_cast<float>(elapsedS);
        if (forceFixedDeltaTime)
        {
            advanceDeltaTime = 1.0f / 120;
        }

        m_scene->advanceAndApply(paused ? 0 : advanceDeltaTime);

        copiesLeft = std::max(copiesLeft, 0);
        copiesAbove = std::max(copiesAbove, 0);
        copiesRight = std::max(copiesRight, 0);
        copiesBelow = std::max(copiesBelow, 0);
        int copyCount =
            (copiesLeft + 1 + copiesRight) * (copiesAbove + 1 + copiesBelow);
        if (copyCount != lastReportedCopyCount ||
            paused != lastReportedPauseState)
        {
            printf("Drawing %i copies of %s%s at %u x %u\n",
                   copyCount,
                   m_rivName.c_str(),
                   paused ? " (paused)" : "",
                   TestingWindow::Get()->width(),
                   TestingWindow::Get()->height());
            lastReportedCopyCount = copyCount;
            lastReportedPauseState = paused;
        }

        auto renderer = TestingWindow::Get()->beginFrame({
            .clearColor = 0xff303030,
            .doClear = true,
            .wireframe = wireframe,
        });

        if (hotloadShaders)
        {
            hotloadShaders = false;
#ifndef RIVE_NO_STD_SYSTEM
            std::system("sh rebuild_shaders.sh /tmp/rive");
            TestingWindow::Get()->hotloadShaders();
#endif
        }

        renderer->save();

        uint32_t width = TestingWindow::Get()->width();
        uint32_t height = TestingWindow::Get()->height();
        for (int i = rotations90; (i & 3) != 0; --i)
        {
            renderer->transform(rive::Mat2D(0, 1, -1, 0, width, 0));
            std::swap(height, width);
        }
        if (zoomLevel != 0)
        {
            float scale = powf(1.25f, zoomLevel);
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
        float spacingPx = spacing * 5 + 150;
        renderer->translate(-spacingPx * copiesLeft, -spacingPx * copiesAbove);
        for (int y = -copiesAbove; y <= copiesBelow; ++y)
        {
            renderer->save();
            for (int x = -copiesLeft; x <= copiesRight; ++x)
            {
                m_artboard->draw(renderer.get());
                renderer->translate(spacingPx, 0);
            }
            renderer->restore();
            renderer->translate(0, spacingPx);
        }
        renderer->restore();

        if (m_fpsText != nullptr)
        {
            // Draw FPS.
            renderer->save();
            renderer->translate(0, 20);
            m_fpsText->render(renderer.get(), m_blackStroke);
            m_fpsText->render(renderer.get(), m_whiteFill);
            renderer->restore();
        }

        renderer->restore();
        TestingWindow::Get()->endFrame();

        // Count FPS.
        ++m_fpsFrames;
        const double elapsedFPSUpdate =
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                timeNow - m_timeLastFPSUpdate)
                .count() /
            1e9; // convert to s
        if (elapsedFPSUpdate >= 2.0)
        {
            double fps = m_fpsFrames / elapsedFPSUpdate;
            printf("[%.3f FPS]\n", fps);

            char fpsRawText[32];
            snprintf(fpsRawText, sizeof(fpsRawText), "   %.1f FPS   ", fps);
            m_fpsText = std::make_unique<rive::RawText>(
                TestingWindow::Get()->factory());
            m_fpsText->maxWidth(width);
#ifdef RIVE_ANDROID
            m_fpsText->align(rive::TextAlign::center);
#else
            m_fpsText->align(rive::TextAlign::right);
#endif
            m_fpsText->sizing(rive::TextSizing::fixed);
            m_fpsText->append(fpsRawText, nullptr, m_roboto, 50.f);

            m_fpsFrames = 0;
            m_timeLastFPSUpdate = timeNow;
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
                    key_pressed(inputEventData.metadata.key);
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

        char key;
        while (TestHarness::Instance().peekChar(key))
        {
            key_pressed(key);
        }
    }

private:
    std::string m_rivName;
    rive::rcp<rive::File> m_file;
    std::unique_ptr<rive::ArtboardInstance> m_artboard;
    std::unique_ptr<rive::Scene> m_scene;

    int lastReportedCopyCount = 0;
    bool lastReportedPauseState = paused;

    rive::rcp<rive::Font> m_roboto;
    rive::rcp<rive::RenderPaint> m_blackStroke;
    rive::rcp<rive::RenderPaint> m_whiteFill;
    std::unique_ptr<rive::RawText> m_fpsText;
    int m_fpsFrames = 0;
    std::chrono::high_resolution_clock::time_point m_timeLastFPSUpdate;
    std::chrono::high_resolution_clock::time_point m_timestampPrevFrame;
};

static Player player;

#if defined(RIVE_IOS) || defined(RIVE_IOS_SIMULATOR)
int player_ios_main(int argc, const char* argv[])
#elif defined(RIVE_ANDROID)
int rive_android_main(int argc, const char* const* argv)
#elif defined(__EMSCRIPTEN__)
int rive_wasm_main(int argc, const char* const* argv)
#else
int main(int argc, const char* argv[])
#endif
{
#ifdef _WIN32
    // Cause stdout and stderr to print immediately without buffering.
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
#endif

    std::string rivName;
    std::vector<uint8_t> rivBytes;
    auto backend =
#ifdef __APPLE__
        TestingWindow::Backend::metal;
#else
        TestingWindow::Backend::vk;
#endif
    auto visibility = TestingWindow::Visibility::fullscreen;
    TestingWindow::BackendParams backendParams;

    for (int i = 0; i < argc; ++i)
    {
        if (strcmp(argv[i], "--test_harness") == 0)
        {
            TestHarness::Instance().init(TCPClient::Connect(argv[++i]), 0);
            if (!TestHarness::Instance().fetchRivFile(rivName, rivBytes))
            {
                fprintf(stderr, "failed to fetch a riv file.");
                abort();
            }
        }
        else if (strcmp(argv[i], "--backend") == 0 ||
                 strcmp(argv[i], "-b") == 0)
        {
            backend = TestingWindow::ParseBackend(argv[++i], &backendParams);
        }
        else if (argv[i][0] == '-' &&
                 argv[i][1] == 'b') // "-bvk" without a space.
        {
            backend = TestingWindow::ParseBackend(argv[i] + 2, &backendParams);
        }
        else if (strcmp(argv[i], "--options") == 0 ||
                 strcmp(argv[i], "-k") == 0)
        {
            for (const char* k = argv[++i]; *k; ++k)
            {
                key_pressed(*k);
            }
        }
        else if (argv[i][0] == '-' &&
                 argv[i][1] == 'k') // "-k1234asdf" without a space.
        {
            for (const char* k = argv[i] + 2; *k; ++k)
            {
                key_pressed(*k);
            }
        }
        else if (strcmp(argv[i], "--window") == 0 || strcmp(argv[i], "-w") == 0)
        {
            visibility = TestingWindow::Visibility::window;
        }
        else
        {
            // No argument name defaults to the source riv.
            if (strcmp(argv[i], "--src") == 0 || strcmp(argv[i], "-s") == 0)
            {
                ++i;
            }
            rivName = argv[i];
            std::ifstream rivStream(rivName, std::ios::binary);
            rivBytes =
                std::vector<uint8_t>(std::istreambuf_iterator<char>(rivStream),
                                     {});
        }
    }

    TestingWindow::Init(backend,
                        backendParams,
                        visibility,
#ifdef RIVE_ANDROID
                        rive_android_app_wait_for_window()
#else
                        reinterpret_cast<void*>(
                            static_cast<intptr_t>(monitorIdx))
#endif
    );

    if (rivBytes.empty())
    {
        fprintf(stderr, "no .riv file specified");
        abort();
    }

    player.init(std::move(rivName), std::move(rivBytes));

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop([]() { player.doFrame(); }, 0, true);
#else
    for (;;)
    {
        player.doFrame();
    }
#endif

    return 0;
}

#endif
