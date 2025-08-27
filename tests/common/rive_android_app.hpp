/*
 * Copyright 2024 Rive
 */

#pragma once

#include <android/native_app_glue/android_native_app_glue.h>

// Blocks until the NaitveActivity's window is initialized.
ANativeWindow* rive_android_app_wait_for_window();

// Polls the android message loop without blocking.
// Returns false if a destroy has been requested and the app should terminate.
bool rive_android_app_poll_once(int timeoutMillis = 0);

// Primary entrypoint for rive android tests. Each individual test is expected
// to implement this method on android (instead of "main()").
extern int rive_android_main(int argc, const char* const* argv);
