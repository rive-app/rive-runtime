#include "viewer/viewer.hpp"
#ifdef SOKOL_GLCORE33
#include "sokol_app.h"
#ifndef GL_SILENCE_DEPRECATION
#define GL_SILENCE_DEPRECATION
#endif
#import "Cocoa/Cocoa.h"
#endif

void bindGraphicsContext()
{
#ifdef SOKOL_GLCORE33
    NSWindow* window = (NSWindow*)sapp_macos_get_window();
    NSOpenGLView* sokolView = (NSOpenGLView*)window.contentView;
    NSOpenGLContext* ctx = [sokolView openGLContext];
    [ctx makeCurrentContext];
#endif
}