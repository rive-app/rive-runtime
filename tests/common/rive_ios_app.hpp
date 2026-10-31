/*
 * Copyright 2026 Rive
 */

#pragma once

#include "common/testing_window.hpp"

void* rive_ios_app_wait_for_window();

// The native drawable size of the app's layer, for headless windows that want
// to match the screen.
void rive_ios_app_native_size(uint32_t* width, uint32_t* height);

bool rive_ios_app_poll_input_event(TestingWindow::InputEventData& eventData);

bool rive_ios_app_should_quit();

void rive_ios_app_wait_while_inactive();
