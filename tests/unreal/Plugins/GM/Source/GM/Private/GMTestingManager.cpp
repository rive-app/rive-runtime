// Fill out your copyright notice in the Description page of Project Settings.

#include "GMTestingManager.h"

#include "IRiveRenderer.h"
#include "IRiveRendererModule.h"
#include "Rive/RiveTextureObject.h"
#include "GM.h"
#include "goldens.hpp"
#if WITH_EDITOR
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#endif
#include "Platform/RenderContextRHIImpl.hpp"

AGMTestingManager::AGMTestingManager()
{
    StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMesh"));
    RootComponent = StaticMeshComponent;
}

void AGMTestingManager::StartTesting()
{
    ENQUEUE_RENDER_COMMAND(GMGoldenMainCommand)
    ([this](FRHICommandListImmediate& RHICmdList) {
        FString CmdLine = FCommandLine::GetOriginal();
        UE_LOG(GM_Log,
               Display,
               TEXT("AGMTestingManager::StartTesting() Start testing with command line "
                    "and %s cwd %s"),
               *CmdLine,
               *FPaths::LaunchDir());

        TArray<FString> CommandLineOptions;
        auto Parsed = CmdLine.ParseIntoArray(CommandLineOptions, TEXT(" "));

        UE_LOG(GM_Log,
               Display,
               TEXT("AGMTestingManager::StartTesting() Processing %i arguments"),
               Parsed);

        int argc = 0;
        // all possible values + some padding
        // we ignore -p since we are always single threaded
        const char* argv[16];
        if (CommandLineOptions.Num() > 1)
        {
            argv[argc++] = TCHAR_TO_ANSI(*CommandLineOptions[0]);
        }
        else
        {
            argv[argc++] = TCHAR_TO_ANSI(*FPaths::LaunchDir());
        }

        for (size_t i = 1; i < CommandLineOptions.Num(); ++i)
        {
            UE_LOG(GM_Log,
                   Display,
                   TEXT("AGMTestingManager::StartTesting() Processing {%s}"),
                   *CommandLineOptions[i]);

            if (CommandLineOptions[i] == TEXT("--test_harness"))
            {
                argv[argc++] = "--test_harness";
                argv[argc++] = TCHAR_TO_ANSI(*CommandLineOptions[++i]);
            }
            else if (CommandLineOptions[i] == TEXT("-o"))
            {
                FString output = FPaths::Combine(FPaths::LaunchDir(), CommandLineOptions[++i]);
                output = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*output);
                argv[argc++] = "--output";
                argv[argc++] = TCHAR_TO_ANSI(*output);
            }
            else if (CommandLineOptions[i] == TEXT("--output"))
            {
                FString output = FPaths::Combine(FPaths::LaunchDir(), CommandLineOptions[++i]);
                output = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*output);
                argv[argc++] = "--output";
                argv[argc++] = TCHAR_TO_ANSI(*output);
            }
            else if (CommandLineOptions[i] == TEXT("-s"))
            {
                FString src = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(
                    *CommandLineOptions[++i]);
                argv[argc++] = "--src";
                argv[argc++] = TCHAR_TO_ANSI(*src);
            }
            else if (CommandLineOptions[i] == TEXT("--src"))
            {
                FString src = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(
                    *CommandLineOptions[++i]);
                argv[argc++] = "--src";
                argv[argc++] = TCHAR_TO_ANSI(*src);
            }
            else if (CommandLineOptions[i] == TEXT("--match"))
            {
                argv[argc++] = "--match";
                argv[argc++] = TCHAR_TO_ANSI(*CommandLineOptions[++i]);
            }
            else if (CommandLineOptions[i] == TEXT("-m"))
            {
                argv[argc++] = "--match";
                argv[argc++] = TCHAR_TO_ANSI(*CommandLineOptions[++i]);
            }
            else if (CommandLineOptions[i] == TEXT("--fast-png"))
            {
                argv[argc++] = "--fast-png";
            }
            else if (CommandLineOptions[i] == TEXT("-f"))
            {
                argv[argc++] = "--fast-png";
            }
            else if (CommandLineOptions[i] == TEXT("--interactive"))
            {
                argv[argc++] = "--interactive";
            }
            else if (CommandLineOptions[i] == TEXT("-i"))
            {
                argv[argc++] = "--interactive";
            }
            else if (CommandLineOptions[i] == TEXT("--backend"))
            {
                ++i;
                UE_LOG(GM_Log,
                       Warning,
                       TEXT("Ingoring backend command because it is hardcoded to rhi"));
            }
            else if (CommandLineOptions[i] == TEXT("-b"))
            {
                ++i;
                UE_LOG(GM_Log,
                       Warning,
                       TEXT("Ingoring backend command because it is hardcoded to rhi"));
            }
            else if (CommandLineOptions[i] == TEXT("--headless"))
            {
                argv[argc++] = "--headless";
            }
            else if (CommandLineOptions[i] == TEXT("-h"))
            {
                argv[argc++] = "--headless";
            }
            else if (CommandLineOptions[i] == TEXT("--verbose"))
            {
                argv[argc++] = "--verbose";
            }
            else if (CommandLineOptions[i] == TEXT("-v"))
            {
                argv[argc++] = "--verbose";
            }
        }

        argv[argc++] = "--backend";
        argv[argc++] = "rhi";

        if (bShouldAddTestArguments)
        {
            UE_LOG(GM_Log,
                   Display,
                   TEXT("AGMTestingManager::InitTesting() gms_main adding test params"));

            argv[argc++] = "--verbose";

            FString ProjectDir = FPaths::ProjectDir();
            ProjectDir =
                IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*ProjectDir);

            argv[argc++] = "--output";
            argv[argc++] = TCHAR_TO_ANSI(*FPaths::Combine(ProjectDir, "output"));

            if (TestingType == EGMTestingType::Golden)
            {
                FString RivePath =
                    FPaths::Combine(FPaths::ProjectDir(), "../../../../gold/rivs/Bear.riv");
                RivePath =
                    IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*RivePath);

                argv[argc++] = "--src";
                argv[argc++] = TCHAR_TO_ANSI(*RivePath);
            }
        }

        FString ArgLog;
        for (size_t i = 0; i < argc; i++)
        {
            ArgLog += argv[i] + FString(" ");
        }

        UE_LOG(GM_Log,
               Display,
               TEXT("AGMTestingManager::StartTesting() starting gms with argc %i argv %s"),
               argc,
               *ArgLog);

        switch (TestingType)
        {
            case EGMTestingType::GM:
                RunGMs(argc, argv);
                break;
            case EGMTestingType::Golden:
                RunGoldens(argc, argv);
                break;
            case EGMTestingType::Both:
                RunGMs(argc, argv);
                RunGoldens(argc, argv);
        }
        // we have to reset this here because both gms and goldens delete the window
        TestingWindow = nullptr;

        AsyncTask(ENamedThreads::GameThread, [this]() {
            if (bShouldExitOnFinish)
            {
#if WITH_EDITOR
                UKismetSystemLibrary::QuitGame(GetWorld(),
                                               UGameplayStatics::GetPlayerController(GetWorld(), 0),
                                               EQuitPreference::Type::Quit,
                                               false);
#else
            FGenericPlatformMisc::RequestExit(false);
#endif
            }
            else
            {
                OnGmTestingFinished.Broadcast();
            }
        });
    });
}

void AGMTestingManager::DeInitTesting() {}

void AGMTestingManager::SetDMTexture(UTexture2DDynamic* Texture, bool MeshIsVisible)
{
    check(DM);
    DM->SetTextureParameterValue("texture", Texture);
    StaticMeshComponent->SetHiddenInGame(!MeshIsVisible, true);
}

void AGMTestingManager::RunGM(UPARAM(ref) FGMData& GM, bool ShouldGenerateDisplayTexture)
{
    TestingWindow->isGoldens = false;
    if (GM.registry_position == nullptr)
    {

        UE_LOG(GM_Log, Error, TEXT("Failed to find gm of name %s"), *GM.Name);
        return;
    }

    if (ShouldGenerateDisplayTexture)
    {
        UE_LOG(GM_Log, Display, TEXT("Generating display texture for gm named %s"), *GM.Name);
        if (GM.Width == 0 || GM.Height == 0)
        {
            size_t width, height;
            if (gms_registry_get_size(GM.registry_position, width, height))
            {
                GM.Width = width;
                GM.Height = height;
            }
            else
            {
                UE_LOG(GM_Log, Error, TEXT("Failed to get size of gm named %s"), *GM.Name);
                return;
            }
        }

        GM.RiveTexture = NewObject<URiveTexture>(GetOuter());
        GM.RiveTexture->OnResourceInitializedOnRenderThread.AddUObject(
            this,
            &AGMTestingManager::RunSpecificGM_RenderThread);

        TestingWindow->RenderTexture = GM.RiveTexture;
        GMToRun = &GM;

        GM.RiveTexture->ResizeRenderTargets(FIntPoint(GM.Width, GM.Height));
    }
}

void AGMTestingManager::RunGolden(FGoldenData& Golden, bool ShouldGenerateDisplayTexture)
{
    TestingWindow->isGoldens = true;
}

void AGMTestingManager::BeginPlay()
{
    DM = StaticMeshComponent->CreateDynamicMaterialInstance(0);

    CustomTestTexture = NewObject<URiveTexture>(GetOuter());

    auto& RenderModule = IRiveRendererModule::Get();
    auto InRenderer = RenderModule.GetRenderer();
    InRenderer->CallOrRegister_OnInitialized(
        IRiveRenderer::FOnRendererInitialized::FDelegate::CreateUObject(
            this,
            &AGMTestingManager::RiveReady));
}

void AGMTestingManager::RiveReady(IRiveRenderer* InRiveRenderer)
{
    Renderer = InRiveRenderer;
    bRiveReady = true;

    // just in case we didnt run gms yet
    if (TestingWindow)
        delete TestingWindow;

    TestingWindow = new UnrealTestingWindow(this, InRiveRenderer);
    ::TestingWindow::Set(TestingWindow);

    if (bShouldGenerateGMList)
    {
        // populate GMList with info
        REGISTRY_HANDLE registery_handle = rivegm::GMRegistry::head();

        while (registery_handle)
        {
            FGMData GMData;
            std::string GMName;
            if (gms_registry_get_name(registery_handle, GMName))
            {
                GMData.Name.AppendChars(GMName.c_str(), GMName.size());
            }

            size_t width, height;
            if (gms_registry_get_size(registery_handle, width, height))
            {
                GMData.Width = width;
                GMData.Height = height;
            }

            GMData.registry_position = registery_handle;

            GMList.Add(GMData);
            registery_handle = gms_registry_get_next(registery_handle);
        }

        UE_LOG(GM_Log, Display, TEXT("GMs ready for testing with %i GMs"), GMList.Num());
    }

    UE_LOG(GM_Log, Display, TEXT("GMs ready for testing with"));

    OnGMTestingReady.Broadcast();
}

void AGMTestingManager::RunGMs(int argc, const char* argv[])
{
    TestingWindow->isGoldens = false;
    int error = gms_main(argc, argv);

    if (error != 0)
    {
        UE_LOG(GM_Log,
               Error,
               TEXT("AGMTestingManager::StartTesting() gms_main failed with error code %i"),
               error);
    }
    else
    {
        UE_LOG(GM_Log,
               Display,
               TEXT("AGMTestingManager::StartTesting() gms_main finished succefully"));
    }
}

void AGMTestingManager::RunGoldens(int argc, const char* argv[])
{
    TestingWindow->isGoldens = true;
    int error = goldens_main(argc, argv);

    if (error != 0)
    {
        UE_LOG(GM_Log,
               Error,
               TEXT("AGMTestingManager::StartTesting() goldens_main failed with error code %i"),
               error);
    }
    else
    {
        UE_LOG(GM_Log,
               Display,
               TEXT("AGMTestingManager::StartTesting() goldens_main finished succefully"));
    }
}

void AGMTestingManager::RunCustomTestClear()
{
    check(CustomTestTexture);
    DM->SetTextureParameterValue(TEXT("texture"), CustomTestTexture);
    CustomTestTexture->OnResourceInitializedOnRenderThread.AddUObject(
        this,
        &AGMTestingManager::RunCustomTestClear_RenderThread);
    CustomTestTexture->ResizeRenderTargets(FIntPoint(256, 256));
}

void AGMTestingManager::RunCustomTestManyPaths(int32 NumberOfPathsToRun)
{
    auto Size = sqrt(NumberOfPathsToRun);
    check(CustomTestTexture);
    check(Size);
    Size = FMath::Clamp(Size, RIVE_MIN_TEX_RESOLUTION, RIVE_MAX_TEX_RESOLUTION);
    DM->SetTextureParameterValue(TEXT("texture"), CustomTestTexture);
    CustomTestTexture->OnResourceInitializedOnRenderThread.AddUObject(
        this,
        &AGMTestingManager::RunCustomTestManyPaths_RenderThread);
    CustomTestTexture->ResizeRenderTargets(FIntPoint(Size, Size));
}

void AGMTestingManager::RunCustomTestClear_RenderThread(FRHICommandListImmediate& RHICmdList,
                                                        FTextureRHIRef& NewResource)
{
    auto context = Renderer->GetRenderContext();
    auto impl = context->static_impl_cast<RenderContextRHIImpl>();
    auto renderTarget = impl->makeRenderTarget(RHICmdList, NewResource);

    context->beginFrame({.renderTargetWidth = NewResource->GetSizeX(),
                         .renderTargetHeight = NewResource->GetSizeY(),
                         .loadAction = rive::gpu::LoadAction::clear,
                         .clearColor = 0xFFFFFF00});

    context->flush({.renderTarget = renderTarget.get()});
}

void AGMTestingManager::RunCustomTestManyPaths_RenderThread(FRHICommandListImmediate& RHICmdList,
                                                            FTextureRHIRef& NewResource)
{
    auto context = Renderer->GetRenderContext();
    auto impl = context->static_impl_cast<RenderContextRHIImpl>();
    auto renderTarget = impl->makeRenderTarget(RHICmdList, NewResource);

    auto width = NewResource->GetSizeX();
    auto height = NewResource->GetSizeY();

    context->beginFrame({.renderTargetWidth = width,
                         .renderTargetHeight = height,
                         .loadAction = rive::gpu::LoadAction::clear,
                         .clearColor = 0xFF000000});

    auto renderer = std::make_unique<rive::RiveRenderer>(context);

    rive::ColorInt Color = 0;

    const int ColorStep = 0xFFFFFF / (width * height);

    for (size_t x = 0; x < width; x++)
    {
        for (size_t y = 0; y < height; y++)
        {
            Color += ColorStep;
            auto strokePaint = context->makeRenderPaint();
            strokePaint->style(rive::RenderPaintStyle::fill);
            strokePaint->color(rive::colorWithAlpha(Color, 0xFF));
            strokePaint->blendMode(rive::BlendMode::srcOver);
            rive::RawPath path;
            path.addRect({static_cast<float>(x),
                          static_cast<float>(y),
                          static_cast<float>(x + 1),
                          static_cast<float>(y + 1)});
            auto renderPath = context->makeRenderPath(path, rive::FillRule::nonZero);
            renderer->drawPath(renderPath.get(), strokePaint.get());
        }
    }

    context->flush({.renderTarget = renderTarget.get()});
}

void AGMTestingManager::RunSpecificGM_RenderThread(FRHICommandListImmediate& RHICmdList,
                                                   FTextureRHIRef& NewResource)
{
    if (GMToRun == nullptr)
    {
        UE_LOG(GM_Log, Error, TEXT("No GM To Run!"));
        return;
    }

    // we are already on the render thread so we don't need to enque a new command
    if (!gms_run_gm(GMToRun->registry_position))
    {
        UE_LOG(GM_Log, Error, TEXT("Failed to run gm named %s"), *GMToRun->Name);
    }

    AsyncTask(ENamedThreads::GameThread, [this, GM = GMToRun]() { SetDMTexture(GM->RiveTexture); });

    // null it out since we already ran it
    GMToRun = nullptr;
}

void UGMDataStatics::ResetGmTexture(FGMData& Data) { Data.RiveTexture = nullptr; }

void UGMDataStatics::ResetGoldenTexture(FGoldenData& Data) { Data.RiveTexture = nullptr; }

void UGMDataStatics::MakeGoldenData(FString PathToRiv, FGoldenData& OutGoldenData, bool& Successful)
{
    auto& FileManager = IFileManager::Get();
    if (!FileManager.FileExists(*PathToRiv))
    {
        Successful = false;
        return;
    }

    OutGoldenData.Name = FPaths::GetBaseFilename(PathToRiv);
    OutGoldenData.FilePath = PathToRiv;
    Successful = true;
}

void UGMDataStatics::GenerateGoldenForDefaultLocation(TArray<FGoldenData>& OutGoldens)
{
    auto& FileManager = IFileManager::Get();

    const auto ProjectFile = FPaths::GetProjectFilePath();
    auto RelativePath = FPaths::Combine(ProjectFile, "../../../../../gold/rivs");
    auto RivDir = FileManager.ConvertToAbsolutePathForExternalAppForRead(*RelativePath);
    UE_LOG(GM_Log, Display, TEXT("Generating Default Golden List for %s"), *RivDir);
    FileManager.IterateDirectoryStat(
        *RivDir,
        [&OutGoldens](const TCHAR* Path, const FFileStatData& Stat) -> bool {
            FGoldenData Data;
            bool success = false;
            UGMDataStatics::MakeGoldenData(Path, Data, success);

            if (success)
            {
                OutGoldens.Add(Data);
            }

            return true;
        });
}
