/*
 * Copyright 2024 Rive
 */

#if defined(RIVE_ANDROID) && !defined(RIVE_UNREAL)

#include "common/rive_android_app.hpp"
#include <vector>

static android_app* _app;
static bool _windowInitialized = false;
// Dimensions the window was at when we initialized it (and therefore built the
// swapchain from). Used to distinguish a genuine live resize from the redundant
// same-size APP_CMD_WINDOW_RESIZED that Android emits during startup.
static int32_t _initialWindowWidth = 0;
static int32_t _initialWindowHeight = 0;

// Parses CLI arguments from the NativeActivity's launch intent and then calls
// rive_android_main().
extern "C" JNIEXPORT void JNICALL android_main(android_app* app)
{
    _app = app;
    _windowInitialized = false;

    _app->onAppCmd = [](android_app* _app, int32_t cmd) {
        // Wait until the window is initialized to call android_player_main.
        if (cmd == APP_CMD_INIT_WINDOW)
        {
            _windowInitialized = true;
            _initialWindowWidth = ANativeWindow_getWidth(_app->window);
            _initialWindowHeight = ANativeWindow_getHeight(_app->window);
        }
        else if (cmd == APP_CMD_WINDOW_RESIZED && _windowInitialized)
        {
            // We build the swapchain once from the initial window size and
            // never recreate it, so a live resize isn't handled: the render
            // surface would be stale (or the swapchain would go out-of-date).
            // Android emits a redundant same-size resize during startup, so
            // only warn when the dimensions actually changed.
            int32_t width = ANativeWindow_getWidth(_app->window);
            int32_t height = ANativeWindow_getHeight(_app->window);
            if (width != _initialWindowWidth || height != _initialWindowHeight)
            {
                fprintf(stderr,
                        "WARNING: window resized from %dx%d to %dx%d after "
                        "startup; the swapchain is not recreated, so rendering "
                        "may be wrong-sized or fail.\n",
                        _initialWindowWidth,
                        _initialWindowHeight,
                        width,
                        height);
            }
        }
    };

    JNIEnv* env;
    _app->activity->vm->AttachCurrentThread(&env, NULL);

    ANativeActivity* nativeActivity = _app->activity;
    jobject javaActivity = nativeActivity->clazz;

    // activity.getIntent()
    jclass activityClass = env->GetObjectClass(javaActivity);
    jmethodID getIntentFn = env->GetMethodID(activityClass,
                                             "getIntent",
                                             "()Landroid/content/Intent;");
    jobject intent = env->CallObjectMethod(javaActivity, getIntentFn);

    // ... .getExtras()
    jclass intentClass = env->FindClass("android/content/Intent");
    jmethodID getExtrasFn =
        env->GetMethodID(intentClass, "getExtras", "()Landroid/os/Bundle;");
    jobject extras = env->CallObjectMethod(intent, getExtrasFn);

    std::vector<char> rawArgs;
    std::vector<const char*> argv;

    // argv[0] is conventionally the program name; rive_android_main (and all
    // other main-like entry points) start parsing at index 1.
    // If we ever care, we could query this (I think using getPackageCodePath),
    // but until then make it an obviously-fake name.
    argv.push_back("fake_program_name");
    if (extras != nullptr)
    {
        // ... .getString("args")
        jclass bundleClass = env->FindClass("android/os/Bundle");
        jmethodID getStringFn =
            env->GetMethodID(bundleClass,
                             "getString",
                             "(Ljava/lang/String;)Ljava/lang/String;");
        jstring args = static_cast<jstring>(
            env->CallObjectMethod(extras,
                                  getStringFn,
                                  env->NewStringUTF("args")));

        if (args != nullptr)
        {
            // Split args by space.
            const char* argsUtf8 = env->GetStringUTFChars(args, 0);
            rawArgs.resize(strlen(argsUtf8) + 1);
            strcpy(rawArgs.data(), argsUtf8);
            env->ReleaseStringUTFChars(args, argsUtf8);
            const char* arg = strtok(rawArgs.data(), " ");
            while (arg != nullptr)
            {
                argv.push_back(arg);
                arg = strtok(nullptr, " ");
            }
        }
    }

    rive_android_main(static_cast<int>(argv.size()), argv.data());

    ANativeActivity_finish(_app->activity);
    _app->activity->vm->DetachCurrentThread();

    while (!_app->destroyRequested)
    {
        rive_android_app_poll_once(-1);
    }
}

ANativeWindow* rive_android_app_wait_for_window()
{
    // Pump messages until the window is initialized.
    while (!_app->destroyRequested && !_windowInitialized)
    {
        rive_android_app_poll_once(-1);
    }

    return _windowInitialized ? _app->window : nullptr;
}

bool rive_android_app_poll_once(int timeoutMillis)
{
    android_poll_source* source = nullptr;
    int result = ALooper_pollOnce(timeoutMillis,
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
        source->process(_app, source);
    }
    return !_app->destroyRequested;
}
#endif
