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
#include <android/native_app_glue/android_native_app_glue.h>

// Called after the window has been initialized.
static int android_player_main(int argc, const char* const* argv, android_app*);

int rive_android_main(int argc, const char* const* argv, android_app* app)
{
    bool windowInitialized = false;
    app->userData = &windowInitialized;
    app->onAppCmd = [](android_app* app, int32_t cmd) {
        // Wait until the window is initialized to call android_player_main.
        if (cmd == APP_CMD_INIT_WINDOW)
        {
            bool* windowInitialized = static_cast<bool*>(app->userData);
            *windowInitialized = true;
        }
    };

    // Pump messages until the window is initialized.
    while (!app->destroyRequested && !windowInitialized)
    {
        android_poll_source* source = nullptr;
        auto result = ALooper_pollOnce(-1,
                                       nullptr,
                                       nullptr,
                                       reinterpret_cast<void**>(&source));
        if (result == ALOOPER_POLL_ERROR)
        {
            fprintf(stderr, "ALooper_pollOnce returned an error");
            abort();
        }

        if (source != nullptr)
        {
            source->process(app, source);
        }
    }

    return windowInitialized ? android_player_main(argc, argv, app) : 0;
}
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
static bool quit = false;
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
        case 'q':
        case '\x03': // ^C
            quit = true;
            break;
        case '\x1b': // Esc
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

#if defined(RIVE_IOS) || defined(RIVE_IOS_SIMULATOR)
int player_ios_main(int argc, const char* argv[])
#elif defined(RIVE_ANDROID)
static int android_player_main(int argc,
                               const char* const* argv,
                               android_app* app)
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
    std::string gpuNameFilter;
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
            backend = TestingWindow::ParseBackend(argv[++i], &gpuNameFilter);
        }
        else if (argv[i][0] == '-' &&
                 argv[i][1] == 'b') // "-bvk" without a space.
        {
            backend = TestingWindow::ParseBackend(argv[i] + 2, &gpuNameFilter);
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
                        visibility,
                        gpuNameFilter,
#ifdef RIVE_ANDROID
                        app->window
#else
                        reinterpret_cast<void*>(
                            static_cast<intptr_t>(monitorIdx))
#endif
    );

    // Load the riv file.
    if (rivBytes.empty())
    {
        fprintf(stderr, "no .riv file specified");
        abort();
    }
    auto file = rive::File::import(rivBytes, TestingWindow::Get()->factory());
    assert(file);
    auto artboard = file->artboardDefault();
    assert(artboard);
    std::unique_ptr<rive::Scene> scene = artboard->defaultStateMachine();
    if (!scene)
    {
        scene = artboard->animationAt(0);
    }
    assert(scene);

    int lastReportedCopyCount = 0;
    bool lastReportedPauseState = paused;

    // Setup FPS.
    auto roboto = HBFont::Decode(assets::roboto_flex_ttf());
    auto blackStroke = TestingWindow::Get()->factory()->makeRenderPaint();
    blackStroke->color(0xff000000);
    blackStroke->style(rive::RenderPaintStyle::stroke);
    blackStroke->thickness(4);
    auto whiteFill = TestingWindow::Get()->factory()->makeRenderPaint();
    whiteFill->color(0xffffffff);
    std::unique_ptr<rive::RawText> fpsText;
    int fpsFrames = 0;
    std::chrono::time_point fpsLastTime =
        std::chrono::high_resolution_clock::now();

    while (!quit && !TestingWindow::Get()->shouldQuit())
    {
        scene->advanceAndApply(paused ? 0 : 1.0 / 120);

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
                   rivName.c_str(),
                   paused ? " (paused)" : "",
                   TestingWindow::Get()->width(),
                   TestingWindow::Get()->height());
            lastReportedCopyCount = copyCount;
            lastReportedPauseState = paused;
        }

        auto renderer =
            TestingWindow::Get()->beginFrame(0xff303030, true, wireframe);
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
                        artboard->bounds());
        float spacingPx = spacing * 5 + 150;
        renderer->translate(-spacingPx * copiesLeft, -spacingPx * copiesAbove);
        for (int y = -copiesAbove; y <= copiesBelow; ++y)
        {
            renderer->save();
            for (int x = -copiesLeft; x <= copiesRight; ++x)
            {
                artboard->draw(renderer.get());
                renderer->translate(spacingPx, 0);
            }
            renderer->restore();
            renderer->translate(0, spacingPx);
        }
        renderer->restore();

        if (fpsText != nullptr)
        {
            // Draw FPS.
            renderer->save();
            renderer->translate(0, 20);
            fpsText->render(renderer.get(), blackStroke);
            fpsText->render(renderer.get(), whiteFill);
            renderer->restore();
        }

        renderer->restore();
        TestingWindow::Get()->endFrame();

        // Count FPS.
        ++fpsFrames;
        std::chrono::time_point now = std::chrono::high_resolution_clock::now();
        auto fpsElapsedMS =
            std::chrono::duration_cast<std::chrono::milliseconds>(now -
                                                                  fpsLastTime)
                .count();
        if (fpsElapsedMS > 2000)
        {
            double fps = 1000.0 * fpsFrames / fpsElapsedMS;
            printf("[%.3f FPS]\n", fps);

            char fpsRawText[32];
            snprintf(fpsRawText, sizeof(fpsRawText), "   %.1f FPS   ", fps);
            fpsText = std::make_unique<rive::RawText>(
                TestingWindow::Get()->factory());
            fpsText->maxWidth(width);
#ifdef RIVE_ANDROID
            fpsText->align(rive::TextAlign::center);
#else
            fpsText->align(rive::TextAlign::right);
#endif
            fpsText->sizing(rive::TextSizing::fixed);
            fpsText->append(fpsRawText, nullptr, roboto, 50.f);

            fpsFrames = 0;
            fpsLastTime = now;
        }

        char key;
        while (TestingWindow::Get()->peekKey(key) ||
               TestHarness::Instance().peekChar(key))
        {
            key_pressed(key);
        }

#ifdef RIVE_ANDROID
        android_poll_source* source = nullptr;
        ALooper_pollOnce(0,
                         nullptr,
                         nullptr,
                         reinterpret_cast<void**>(&source));
        if (source != nullptr)
        {
            source->process(app, source);
        }
        if (app->destroyRequested)
        {
            break;
        }
#endif
    }

    printf("\nShutting down\n");
    TestingWindow::Destroy(); // Exercise our PLS teardown process now that
                              // we're done.
    TestHarness::Instance().shutdown();
    return 0;
}

#endif
