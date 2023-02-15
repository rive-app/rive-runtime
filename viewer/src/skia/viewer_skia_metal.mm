#if defined(RIVE_RENDERER_SKIA) && defined(SK_METAL)
#include "viewer/viewer.hpp"
#include "sokol_app.h"
#include "sokol_gfx.h"

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#include "mtl/GrMtlBackendContext.h"
#include "mtl/GrMtlTypes.h"
#import <QuartzCore/CAMetalLayer.h>
#import "Cocoa/Cocoa.h"

id<MTLCommandQueue> commandQueue;
id<CAMetalDrawable> drawable;
GrMtlTextureInfo mtlTexture;
MTKView* skiaView;
NSView* contentView;

typedef NS_OPTIONS(NSUInteger, UIViewAutoresizing) {
    UIViewAutoresizingNone = 0,
    UIViewAutoresizingFlexibleLeftMargin = 1 << 0,
    UIViewAutoresizingFlexibleWidth = 1 << 1,
    UIViewAutoresizingFlexibleRightMargin = 1 << 2,
    UIViewAutoresizingFlexibleTopMargin = 1 << 3,
    UIViewAutoresizingFlexibleHeight = 1 << 4,
    UIViewAutoresizingFlexibleBottomMargin = 1 << 5
};

sk_sp<GrDirectContext> makeSkiaContext()
{
    // This is a little tricky...when using Metal we need to divorce the two
    // views so we don't get contention between Sokol drawing (mostly for ImGui)
    // with Metal and Skia drawing with Metal. I couldn't find a good way to let
    // them share a command queue, so drawing to two separate Metal Layers is
    // the next best thing.
    id<MTLDevice> device = (id<MTLDevice>)sg_mtl_device();
    commandQueue = [device newCommandQueue];

    NSWindow* window = (NSWindow*)sapp_macos_get_window();

    // Add a new metal view to our window.
    skiaView = [[MTKView alloc] init];
    skiaView.device = device;
    skiaView.autoresizingMask =
        (UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight);
    [skiaView setWantsLayer:YES];

    // Grab the current contentView which is the default view Sokol App creates.
    NSView* sokolView = window.contentView;
    sokolView.autoresizingMask =
        (UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight);

    // Make a new contentView (root container).
    contentView = [[NSView alloc] init];
    contentView.frame = sokolView.bounds;
    skiaView.frame = sokolView.bounds;
    window.contentView = contentView;

    // Add Sokol and Skia views to it. Make sure to layer Sokol over Skia.
    [contentView addSubview:skiaView];
    [contentView addSubview:sokolView];
    // Make sure Sokol view is transparent so ImGui can draw over our Skia
    // content.
    sokolView.layer.opaque = false;

    return GrDirectContext::MakeMetal(device, commandQueue);
}

sk_sp<SkSurface> makeSkiaSurface(GrDirectContext* context, int width, int height)
{
    NSView* view = skiaView;
    CAMetalLayer* layer = (CAMetalLayer*)view.layer;

    drawable = [layer nextDrawable];
    GrMtlTextureInfo fbInfo;
    fbInfo.fTexture.retain((const void*)(drawable.texture));
    GrBackendRenderTarget renderTarget =
        GrBackendRenderTarget(width, height, 1 /* sample count/MSAA */, fbInfo);

    return SkSurface::MakeFromBackendRenderTarget(
        context, renderTarget, kTopLeft_GrSurfaceOrigin, kBGRA_8888_SkColorType, nullptr, nullptr);
}

void skiaPresentSurface(sk_sp<SkSurface> surface)
{
    id<MTLCommandBuffer> commandBuffer = [(id<MTLCommandQueue>)commandQueue commandBuffer];
    commandBuffer.label = @"Present";
    [commandBuffer presentDrawable:(id<CAMetalDrawable>)drawable];
    [commandBuffer commit];
}

#endif