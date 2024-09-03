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
        auto result = ALooper_pollOnce(-1, nullptr, nullptr, reinterpret_cast<void**>(&source));
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

#if defined(RIVE_IOS) || defined(RIVE_IOS_SIMULATOR)
int player_ios_main(int argc, const char* argv[])
#elif defined(RIVE_ANDROID)
static int android_player_main(int argc, const char* const* argv, android_app* app)
#else
int main(int argc, const char* argv[])
#endif
{
#ifdef _WIN32
    // Cause stdout and stderr to print immediately without buffering.
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
#endif
    auto backend = TestingWindow::Backend::gl;
    std::string rivName;
    std::string gpuNameFilter;
    std::vector<uint8_t> rivBytes;
    int copiesLeft = 0;
    int copiesAbove = 0;
    int copiesRight = 0;
    int copiesBelow = 0;
    float spacing = 150;
    bool pause = false;
    int rotations90 = 0;
    int monitorIdx = 0;
    int zoomLevel = 0;
    for (int i = 0; i < argc; ++i)
    {
        if (strcmp(argv[i], "--output") == 0)
        {
            TestHarness::Instance().init(argv[++i], "play", 0);
        }
        else if (strcmp(argv[i], "--src") == 0)
        {
            auto rivClient = TCPClient::Connect(argv[++i]);

            // Receive the .riv.
            rivName = rivClient->recvString();
            uint32_t fileSize = rivClient->recv4();
            rivBytes.resize(fileSize);
            rivClient->recvall(rivBytes.data(), fileSize);

            // Close the connection.
            rivClient->send4(TCPClient::HANDSHAKE_TOKEN);
            uint32_t shutdown RIVE_MAYBE_UNUSED = rivClient->recv4();
            assert(shutdown == TCPClient::SHUTDOWN_TOKEN);
        }
        else if (strcmp(argv[i], "--backend") == 0)
        {
            backend = TestingWindow::ParseBackend(argv[++i], &gpuNameFilter);
        }
        else if (strcmp(argv[i], "--options") == 0)
        {
            for (const char* k = argv[++i]; *k; ++k)
            {
                int multiplier = 1, multiplierBytesRead = 0;
                sscanf(k, "%d%n", &multiplier, &multiplierBytesRead);
                k += multiplierBytesRead;
                switch (*k)
                {
                    case 'h':
                        copiesLeft += multiplier;
                        break;
                    case 'l':
                        copiesRight += multiplier;
                        break;
                    case 'H':
                    case 'L':
                        copiesLeft += multiplier;
                        copiesRight += multiplier;
                        break;
                    case 'j':
                        copiesBelow += multiplier;
                        break;
                    case 'k':
                        copiesAbove += multiplier;
                        break;
                    case 'J':
                    case 'K':
                        copiesAbove += multiplier;
                        copiesBelow += multiplier;
                        break;
                    case 's':
                        spacing = multiplier;
                        break;
                    case 'p':
                        pause = true;
                        break;
                    case 'r':
                        rotations90 += multiplier / 90;
                        break;
                    case 'R':
                        rotations90 -= multiplier / 90;
                        break;
                    case 'm':
                        monitorIdx = multiplier;
                        break;
                    case 'Z':
                        zoomLevel += multiplier;
                        break;
                    case 'z':
                        zoomLevel -= multiplier;
                        break;
                    default:
                        fprintf(stderr, "invalid option: %c\n", *k);
                        abort();
                        break;
                }
            }
        }
    }

    TestingWindow::Init(backend,
                        TestingWindow::Visibility::fullscreen,
                        gpuNameFilter,
#ifdef RIVE_ANDROID
                        app->window
#else
                        reinterpret_cast<void*>(static_cast<intptr_t>(monitorIdx))
#endif
    );

    // Load the file.
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

    printf("Drawing %i copies of %s%s at %u x %u\n",
           (copiesLeft + 1 + copiesRight) * (copiesAbove + 1 + copiesBelow),
           rivName.c_str(),
           pause ? " (paused)" : "",
           TestingWindow::Get()->width(),
           TestingWindow::Get()->height());

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
    std::chrono::time_point fpsLastTime = std::chrono::high_resolution_clock::now();

    while (!TestingWindow::Get()->shouldQuit())
    {
        scene->advanceAndApply(pause ? 0 : 1.0 / 120);

        auto renderer = TestingWindow::Get()->beginFrame(0xff303030);
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
            float scale = powf(1.5f, zoomLevel);
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
        renderer->translate(-spacing * copiesLeft, -spacing * copiesAbove);
        for (int y = -copiesAbove; y <= copiesBelow; ++y)
        {
            renderer->save();
            for (int x = -copiesLeft; x <= copiesRight; ++x)
            {
                artboard->draw(renderer.get());
                renderer->translate(spacing, 0);
            }
            renderer->restore();
            renderer->translate(0, spacing);
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
            std::chrono::duration_cast<std::chrono::milliseconds>(now - fpsLastTime).count();
        if (fpsElapsedMS > 2000)
        {
            double fps = 1000.0 * fpsFrames / fpsElapsedMS;
            printf("[%.3f FPS]\n", fps);

            char fpsRawText[32];
            snprintf(fpsRawText, sizeof(fpsRawText), "   %.1f FPS   ", fps);
            fpsText = std::make_unique<rive::RawText>(TestingWindow::Get()->factory());
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

#ifdef RIVE_ANDROID
        android_poll_source* source = nullptr;
        ALooper_pollOnce(0, nullptr, nullptr, reinterpret_cast<void**>(&source));
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

    TestHarness::Instance().shutdown();
    return 0;
}

#endif
