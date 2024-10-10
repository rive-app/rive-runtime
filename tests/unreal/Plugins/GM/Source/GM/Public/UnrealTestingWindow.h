#pragma once
#include "rive/refcnt.hpp"
#include "Rive/RiveTexture.h"
struct FGMData;
class IRiveRenderer;
class IRiveRenderTarget;
class URiveTexture;
THIRD_PARTY_INCLUDES_START
#include "gms.hpp"
THIRD_PARTY_INCLUDES_END

/*
 * UnrealTestingWindow is the testing window implementation for rhi intended to be used with gms
 * unit testing. This is assumed to be on the Render thread. If used on the Game thread it will
 * cause unexpected behaviour.
 */

class UnrealTestingWindow : public TestingWindow
{
public:
    UnrealTestingWindow(UObject* WorldContextObject, IRiveRenderer* RenderTarget);

    virtual void resize(int width, int height) override;

    virtual rive::Factory* factory() override;
    virtual std::unique_ptr<rive::Renderer> beginFrame(uint32_t clearColor,
                                                       bool doClear = true) override;
    virtual void endFrame(std::vector<uint8_t>* pixelData = nullptr) override;

    virtual rive::gpu::RenderContext* renderContext() const override;
    virtual rive::gpu::RenderTarget* renderTarget() const override;
    virtual void flushPLSContext() override;

    virtual bool peekKey(char& key) override;
    virtual char getKey() override;
    virtual bool shouldQuit() const override;

    bool isGoldens = false;

    // texture used for rendering GM
    URiveTexture* RenderTexture = nullptr;

private:
    // we don't reference count this here because a UObject owns this class and the render target
    IRiveRenderer* RiveRenderer;

    rive::gpu::RenderContext* RenderContext;
    rive::rcp<rive::gpu::RenderTarget> RenderTarget;
    // I am not allowed to both render to and read from the same texture, so i'm going to copy to
    // this one and read from here
    FTexture2DRHIRef CopyDestTexture;

    UObject* WorldContextObject;
};
