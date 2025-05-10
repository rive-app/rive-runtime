/*
 * Copyright 2023 Rive
 */

#include "gm.hpp"

using namespace rivegm;
using namespace rive;

DEF_SIMPLE_GM_WITH_CLEAR_COLOR(emptyclear, 0xff00ff00, 64, 64, renderer) {}

// This test ensures that PLS gets properly initialized on
// GL_EXT_shader_pixel_local_storage with a transparent-black clear color.
//
// The extension spec makes a vague hint in Issue #4 that one could clear PLS to
// 0 by issuing a glClear with a zero clear value:
//
//   "This makes the value effectively undefined unless the framebuffer has been
//    cleared to zero."
//
// However, Issue #5 may go back on the suggestion from #4 and require
// applications to always initialize PLS with a fullscreen draw.
//
// https://registry.khronos.org/OpenGL/extensions/EXT/EXT_shader_pixel_local_storage.txt
//
// Either way, we are observing on some older ARM Mali devices that the glClear
// approach does not always initialize PLS, resulting in a rainbow-colored
// QR-code-like pattern. Zero-initializing PLS with a draw instead of glClear
// fixes the issue.
//
// Making an empty render pass with a transparent clear color has a high success
// rate in reproducing the artifact.
//
// See: https://github.com/rive-app/rive-react-native/issues/279
DEF_SIMPLE_GM_WITH_CLEAR_COLOR(emptytransparentclear,
                               0x00000000,
                               320,
                               320,
                               renderer)
{}
