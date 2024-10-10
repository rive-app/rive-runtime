#include "UnrealTestingWindow.h"

#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetInputLibrary.h"
#include "RiveRenderer/Public/IRiveRenderer.h"
#include "Platform/RenderContextRHIImpl.hpp"
#include "Rive/RiveTexture.h"
#include "GMTestingManager.h"
#include <goldens.hpp>

#include "Misc/EngineVersionComparison.h"

#if UE_VERSION_OLDER_THAN(5, 4, 0)
#define CREATE_TEXTURE(RHICmdList, Desc) RHICreateTexture(Desc)
#else // UE_VERSION_OLDER_THAN (5, 4,0)
#define CREATE_TEXTURE(RHICmdList, Desc) RHICmdList.CreateTexture(Desc)
#endif

UnrealTestingWindow::UnrealTestingWindow(UObject* WorldContextObject, IRiveRenderer* RiveRenderer) :
    RiveRenderer(RiveRenderer),
    RenderContext(RiveRenderer->GetRenderContext()),
    WorldContextObject(WorldContextObject)
{}

void UnrealTestingWindow::resize(int width, int height)
{
    TestingWindow::resize(width, height);

    auto impl = RenderContext->static_impl_cast<RenderContextRHIImpl>();
    auto& RHICmdList = GRHICommandList.GetImmediateCommandList();

    if (RenderTexture)
    {
        // use the render texture created external for visibility
        check(RenderTexture->Size.X == width);
        check(RenderTexture->Size.Y == height);
        auto Texture = RenderTexture->GetResource()->GetTexture2DRHI();
        RenderTarget = impl->makeRenderTarget(RHICmdList, Texture);
    }
    else
    {
        FRHITextureCreateDesc Desc =
            FRHITextureCreateDesc::Create2D(TEXT("RiveTestingWindowRenderTarget"),
                                            width,
                                            height,
                                            EPixelFormat::PF_R8G8B8A8);
        Desc.SetFlags(ETextureCreateFlags::UAV | ETextureCreateFlags::Dynamic |
                      ETextureCreateFlags::ShaderResource | ETextureCreateFlags::RenderTargetable);
        FTexture2DRHIRef Texture = CREATE_TEXTURE(RHICmdList, Desc);
        RenderTarget = impl->makeRenderTarget(RHICmdList, Texture);
    }

    FRHITextureCreateDesc CopyDesc =
        FRHITextureCreateDesc::Create2D(TEXT("RiveTestingWindowCopyDest"),
                                        width,
                                        height,
                                        EPixelFormat::PF_R8G8B8A8);
    CopyDesc.SetFlags(ETextureCreateFlags::CPUReadback);
    CopyDestTexture = CREATE_TEXTURE(RHICmdList, CopyDesc);
}

rive::Factory* UnrealTestingWindow::factory() { return RenderContext; }

std::unique_ptr<rive::Renderer> UnrealTestingWindow::beginFrame(uint32_t clearColor, bool doClear)
{
    rive::gpu::RenderContext::FrameDescriptor FrameDescriptor;
    FrameDescriptor.renderTargetWidth = width();
    FrameDescriptor.renderTargetHeight = height();
    FrameDescriptor.loadAction =
        doClear ? rive::gpu::LoadAction::clear : rive::gpu::LoadAction::preserveRenderTarget;
    FrameDescriptor.clearColor = clearColor;
    FrameDescriptor.wireframe = false;
    FrameDescriptor.fillsDisabled = false;
    FrameDescriptor.strokesDisabled = false;

    RenderContext->beginFrame(std::move(FrameDescriptor));
    return std::make_unique<rive::RiveRenderer>(RenderContext);
}

void UnrealTestingWindow::endFrame(std::vector<uint8_t>* pixelData)
{
    RenderContext->flush({RenderTarget.get()});

    if (pixelData == nullptr)
        return;

    const size_t size = m_height * m_width * 4;

    pixelData->resize(size);

    check(pixelData->size() == size);

    auto& RHICmdList = GRHICommandList.GetImmediateCommandList();

    auto RHIRenderTarget = static_cast<RenderTargetRHI*>(RenderTarget.get());
    auto RenderTargetTexture = RHIRenderTarget->texture();

    auto Fence = GDynamicRHI->RHICreateGPUFence(TEXT("GM_FLUSH_FENCE"));
    TransitionAndCopyTexture(RHICmdList,
                             RenderTargetTexture,
                             CopyDestTexture,
                             FRHICopyTextureInfo());

    RHICmdList.Transition(
        FRHITransitionInfo(CopyDestTexture, ERHIAccess::Unknown, ERHIAccess::CPURead));

    RHICmdList.WriteGPUFence(Fence);

    void* Data = nullptr;
    int32 Width;
    int32 Height;
    RHICmdList.MapStagingSurface(CopyDestTexture, Fence.GetReference(), Data, Width, Height);

    check(Width >= static_cast<int32>(m_width));
    check(Height >= static_cast<int32>(m_height));

    uint32 DestStride = Width * 4;
    for (uint32_t y = 0; y < m_height; ++y)
    {
        uint8_t* src = static_cast<uint8_t*>(Data) + (DestStride * y);
        uint8_t* dst = pixelData->data() + ((m_height - y - 1) * m_width * 4);

        memcpy(dst, src, m_width * 4);
    }

    RHICmdList.UnmapStagingSurface(CopyDestTexture);
}

rive::gpu::RenderContext* UnrealTestingWindow::renderContext() const { return RenderContext; }

rive::gpu::RenderTarget* UnrealTestingWindow::renderTarget() const { return RenderTarget.get(); }

void UnrealTestingWindow::flushPLSContext() { RenderContext->flush({RenderTarget.get()}); }

bool UnrealTestingWindow::peekKey(char& key)
{
    auto controller = UGameplayStatics::GetPlayerController(WorldContextObject, 0);
    return controller->GetInputAnalogKeyState(EKeys::SpaceBar) != 0;
}

char UnrealTestingWindow::getKey()
{
    char key = ' ';
    while (!peekKey(key))
    {
        if (shouldQuit())
        {
            printf("Window terminated by user.\n");
            exit(0);
        }
    }

    return key;
}

bool UnrealTestingWindow::shouldQuit() const { return false; }