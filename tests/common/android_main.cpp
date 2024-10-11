/*
 * Copyright 2024 Rive
 */

#ifdef RIVE_ANDROID

#include <android/native_app_glue/android_native_app_glue.h>
#include <vector>

// Primary entrypoint for rive android tools.
extern int rive_android_main(int argc,
                             const char* const* argv,
                             android_app* app);

// Parses CLI arguments from the NativeActivity's launch intent and then calls
// rive_android_main().
extern "C" JNIEXPORT void JNICALL android_main(android_app* app)
{
    JNIEnv* env;
    app->activity->vm->AttachCurrentThread(&env, NULL);

    ANativeActivity* nativeActivity = app->activity;
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

    // ... .getString("args")
    jclass bundleClass = env->FindClass("android/os/Bundle");
    jmethodID getStringFn =
        env->GetMethodID(bundleClass,
                         "getString",
                         "(Ljava/lang/String;)Ljava/lang/String;");
    jstring args = static_cast<jstring>(
        env->CallObjectMethod(extras, getStringFn, env->NewStringUTF("args")));

    // Split args by space.
    const char* argsUtf8 = env->GetStringUTFChars(args, 0);
    std::vector<char> rawArgs;
    rawArgs.resize(strlen(argsUtf8) + 1);
    strcpy(rawArgs.data(), argsUtf8);
    env->ReleaseStringUTFChars(args, argsUtf8);
    const char* arg = strtok(rawArgs.data(), " ");
    std::vector<const char*> argv;
    while (arg != nullptr)
    {
        argv.push_back(arg);
        arg = strtok(nullptr, " ");
    }

    rive_android_main(static_cast<int>(argv.size()), argv.data(), app);
}
#endif
