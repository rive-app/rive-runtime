// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Rive/RiveTexture.h"
#include "UObject/Object.h"
#include "UnrealTestingWindow.h"
#include "GMTestingManager.generated.h"

UENUM(Blueprintable, BlueprintType)
enum class EGMTestingType : uint8
{
    GM,
    Golden,
    Both
};

USTRUCT(Blueprintable, BlueprintType)
struct GM_API FGMData
{
    GENERATED_BODY()
    // Width of the GM gets populate when GM list is created
    UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
    int32 Width = 0;
    // Height of the GM gets populate when GM list is created
    UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
    int32 Height = 0;
    // transient texture that gets deleted after this gm is removed from view to
    // save memory
    UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
    URiveTexture* RiveTexture = nullptr;
    // the name of the golden
    UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
    FString Name;
    // opaque handle to GM registry u accessed and provided by gms.dll
    const void* registry_position = nullptr;
};

USTRUCT(Blueprintable, BlueprintType)
struct GM_API FGoldenData
{
    GENERATED_BODY()

    // this is only populated after the first time this golden has been drawn
    UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
    int32 Width = 0;

    // this is only populated after the first time this golden has been drawn
    UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
    int32 Height = 0;

    // transient texture that gets deleted after this golden is removed from
    // view to save memory
    UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
    URiveTexture* RiveTexture = nullptr;

    // base file name for then golden
    UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
    FString Name;

    // full absolute path to golden
    UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
    FString FilePath;
};

UCLASS()
class GM_API UGMDataStatics : public UObject
{
    GENERATED_BODY()
public:
    // Resets the RiveTexture to nullptr to release the memory
    UFUNCTION(BlueprintCallable)
    static void ResetGmTexture(UPARAM(ref) FGMData& Data);

    // Resets the RiveTexture to nullptr to release the memory
    UFUNCTION(BlueprintCallable)
    static void ResetGoldenTexture(UPARAM(ref) FGoldenData& Data);
    // Create a new golden with minimal info, verifying the path exists
    UFUNCTION(BlueprintCallable)
    static void MakeGoldenData(FString PathToRiv,
                               FGoldenData& OutGoldenData,
                               bool& Successful);
    // Create Goldens using assumed launch location is the Unreal Project root
    // directory, like running from editor
    UFUNCTION(BlueprintCallable)
    static void GenerateGoldenForDefaultLocation(
        TArray<FGoldenData>& OutGoldens);
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGMTestingReady);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGMTestingFinished);
/**
 *
 */
UCLASS()
class GM_API AGMTestingManager : public AActor
{
    GENERATED_BODY()
public:
    AGMTestingManager();

    UFUNCTION(BlueprintCallable)
    void StartTesting();
    UFUNCTION(BlueprintCallable)
    void DeInitTesting();

    UFUNCTION(BlueprintCallable)
    void SetDMTexture(UTexture2DDynamic* Texture, bool MeshIsVisible = true);

    /*
     * Runs the given GM through gms.dll
     * If @ShouldGenerateDisplayTexture is true, a new RiveTexture will be
     * created and stored in GM That will be rendered to assuming it will be
     * used to display the finished GM
     */
    UFUNCTION(BlueprintCallable)
    void RunGM(UPARAM(ref) FGMData& GM,
               bool ShouldGenerateDisplayTexture = true);

    /*
     * Runs the given Golden through goldens_main
     * If @ShouldGenerateDisplayTexture is true, a new RiveTexture will be
     * created and stored in Golden That will be rendered to assuming it will be
     * used to display the finished Golden
     */
    UFUNCTION(BlueprintCallable)
    void RunGolden(UPARAM(ref) FGoldenData& Golden,
                   bool ShouldGenerateDisplayTexture = true);

    UFUNCTION(BlueprintCallable)
    void RunCustomTestClear();

    UFUNCTION(BlueprintCallable)
    void RunCustomTestManyPaths(int32 NumberOfPathsToRun);

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    bool bShouldAddTestArguments;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    bool bShouldExitOnFinish;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    bool bShouldGenerateGMList;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "GM")
    FGMTestingReady OnGMTestingReady;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "GM")
    FGMTestingFinished OnGmTestingFinished;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "GM")
    EGMTestingType TestingType = EGMTestingType::GM;

protected:
    UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
    UStaticMeshComponent* StaticMeshComponent;

    UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
    TArray<FGMData> GMList;

protected:
    virtual void BeginPlay() override;
    void RiveReady(IRiveRenderer* InRiveRenderer);

private:
    void RunGMs(int argc, const char* argv[]);
    void RunGoldens(int argc, const char* argv[]);

    void RunCustomTestClear_RenderThread(FRHICommandListImmediate& RHICmdList,
                                         FTextureRHIRef& NewResource);
    void RunCustomTestManyPaths_RenderThread(
        FRHICommandListImmediate& RHICmdList,
        FTextureRHIRef& NewResource);
    void RunSpecificGM_RenderThread(FRHICommandListImmediate& RHICmdList,
                                    FTextureRHIRef& NewResource);
    // GMRan in RunSpecificGM_RenderThread and set by RunGM
    FGMData* GMToRun = nullptr;

    // this is managed by the GM lib. so we just reset this to null after
    // running gms_main
    UnrealTestingWindow* TestingWindow;

    UPROPERTY()
    URiveTexture* CustomTestTexture;

    UPROPERTY()
    UMaterialInstanceDynamic* DM;

    IRiveRenderer* Renderer;

    bool bRiveReady;
    bool bResourceReady;
};
