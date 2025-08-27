/*
 * Copyright 2024 Rive
 */

#if defined(RIVE_ANDROID) && !defined(RIVE_UNREAL)

#include "common/rive_android_app.hpp"
#include <vector>

static android_app* _app;
static bool _windowInitialized = false;

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
