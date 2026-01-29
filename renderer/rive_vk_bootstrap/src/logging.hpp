/*
 * Copyright 2025 Rive
 */

#pragma once

#include <stdio.h>

#ifdef RIVE_ANDROID
#include <android/log.h>
#endif

// TODO: These probably want to be made more generally available across the
// whole renderer and updated to log nicely for more platforms (like Mac)
#if defined(__ANDROID__)
#define LOG_INFO_LINE(FORMAT, ...)                                             \
    [](auto&&... args) {                                                       \
        printf(FORMAT "\n", std::forward<decltype(args)>(args)...);            \
        __android_log_print(ANDROID_LOG_INFO,                                  \
                            "rive_android_tests",                              \
                            FORMAT,                                            \
                            std::forward<decltype(args)>(args)...);            \
    }(__VA_ARGS__)

// Send errors to stderr and the Android log, just for redundancy in case one or
// the other gets dropped.
#define LOG_ERROR_LINE(FORMAT, ...)                                            \
    [](auto&&... args) {                                                       \
        fprintf(stderr, FORMAT "\n", std::forward<decltype(args)>(args)...);   \
        __android_log_print(ANDROID_LOG_ERROR,                                 \
                            "rive_android_tests",                              \
                            FORMAT,                                            \
                            std::forward<decltype(args)>(args)...);            \
    }(__VA_ARGS__)
#else
// With C++20 (specifically with __VA_OPT__), these could just be:
// #define LOG_INFO_LINE(FORMAT, ...) \
//    printf(FORMAT "\n" __VA_OPT__(,) __VA_ARGS__)
// #define LOG_ERROR_LINE(FORMAT, ...) \
//    fprintf(stderr, FORMAT "\n" __VA_OPT__(,) __VA_ARGS__)
//
// But without, this still needs to be wrapped up in a lambda so that the
// __VA_ARGS__ can be sent in a way that works fine when it's empty.
#define LOG_INFO_LINE(FORMAT, ...)                                             \
    [](auto&&... args) {                                                       \
        printf(FORMAT "\n", std::forward<decltype(args)>(args)...);            \
    }(__VA_ARGS__)

#define LOG_ERROR_LINE(FORMAT, ...)                                            \
    [](auto&&... args) {                                                       \
        fprintf(stderr, FORMAT "\n", std::forward<decltype(args)>(args)...);   \
    }(__VA_ARGS__)

#endif

#define LOG_VULKAN_ERROR_LINE(vkResult, message, ...)                          \
    /* Do a little lambda-based shenanigan to allow this to work even when     \
       there are no variadic arguments */                                      \
    [&](auto&&... args) {                                                      \
        LOG_ERROR_LINE(message "  Error code: %s",                             \
                       args...,                                                \
                       rive::gpu::vkutil::string_from_vk_result(vkResult));    \
    }(__VA_ARGS__)
