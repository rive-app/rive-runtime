/*
 * Copyright 2026 Rive
 */

#pragma once

#include "common/testing_window.hpp"

void* rive_ios_app_wait_for_window();

bool rive_ios_app_poll_input_event(TestingWindow::InputEventData& eventData);

bool rive_ios_app_should_quit();

void rive_ios_app_wait_while_inactive();
