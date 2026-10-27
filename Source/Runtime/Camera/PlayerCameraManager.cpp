#include "EnginePCH.h"
#include "PlayerCameraManager.h"

#include "Camera/CameraComponent.h"
#include "Camera/CameraActor.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Components/ActorComponent.h"

// ------------------------------------------------------------
// FViewTarget
// ------------------------------------------------------------

bool FViewTarget::SetTarget(UCameraComponent* InCamera)
{
    Reset();
    if (!InCamera)
    {
        return false;
    }

    AActor* Owner = InCamera->GetOwner();
    if (!Owner)
    {
        return false;
    }
    
    TargetActor = Owner;
    TargetCamera = InCamera;

    TargetActorUUID = Owner->GetUUID();
    TargetCameraUUID = InCamera->GetUUID();

    POV = InCamera->GetCameraPOV();

    if (!POV.IsValid())
    {
        Reset();
        return false;
    }
    return true;
}

bool FViewTarget::IsValidForWorld(const UWorld* InWorld) const
{
    if (!InWorld || !TargetActor || !TargetCamera)
    {
        return false;
    }

    bool bActorValid = InWorld->IsActorInWorld(TargetActor, TargetActorUUID);

    // 기존 MainCamera는 Level에 등록되지 않고 World가 별도로 관리함
    if (!bActorValid && InWorld->GetMainCamera() == TargetActor)
    {
        bActorValid = TargetActor->GetUUID() == TargetActorUUID && TargetActor->GetWorld() == InWorld;
    }

    if (!bActorValid)
    {
        return false;
    }

    if (TargetActor->GetUUID() != TargetActorUUID || TargetActor->GetWorld() != InWorld)
    {
        return false;
    }

    for (UActorComponent* Component : TargetActor->GetComponents())
    {
        if (Component != TargetCamera)
        {
            continue;
        }

        if (Component->GetUUID() != TargetCameraUUID || Component->GetOwner() != TargetActor || Component->GetWorld() != InWorld)
        {
            return false;
        }

        return TargetCamera->GetCameraPOV().IsValid();
    }
}

void FViewTarget::Reset()
{
    TargetActor = nullptr;
    TargetCamera = nullptr;

    TargetActorUUID = 0;
    TargetCameraUUID = 0;

    POV = FCameraPOV{};
}

// ------------------------------------------------------------
// APlayerCameraManager
// ------------------------------------------------------------

APlayerCameraManager::APlayerCameraManager()
{
    // 추후 Manager에서 시간에 따른 상태 갱신 담당
    PrimaryActorTick.bCanEverTick = true;
}

bool APlayerCameraManager::InitalizeForPlayer(APlayerController* InPlayerController)
{
    if (!InPlayerController || InPlayerController->GetWorld() != GetWorld())
    {
        return false;
    }
    PlayerController = InPlayerController;

    RefreshViewTarget();

    return true;
}

void APlayerCameraManager::BeginPlay()
{
    Super::BeginPlay();
    RefreshViewTarget();
}

void APlayerCameraManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    RefreshViewTarget();
}

void APlayerCameraManager::EndPlay()
{
    ViewTarget.Reset();
    PlayerController = nullptr;

    Super::EndPlay();
}

bool APlayerCameraManager::SetViewTarget(UCameraComponent* InCamera)
{
    UWorld* World = GetWorld();

    if (!World || !InCamera || InCamera->GetWorld() != World)
    {
        return false;
    }

    FViewTarget NewTarget;

    if (!NewTarget.SetTarget(InCamera) || !NewTarget.IsValidForWorld(World))
    {
        return false;
    }

    ViewTarget = NewTarget;

    return true;
}

bool APlayerCameraManager::RefreshViewTarget()
{
    UWorld* World = GetWorld();

    if (!World)
    {
        ViewTarget.Reset();
        return false;
    }

    if (ViewTarget.IsValidForWorld(World))
    {
        ViewTarget.POV = ViewTarget.TargetCamera->GetCameraPOV();
        return ViewTarget.POV.IsValid();
    }

    ViewTarget.Reset();

    // 1순위 플레이어 카메라 
    UCameraComponent* Camera = World->GetPlayerCamera();

    // 2순위 기존 World MainCamera
    if (!Camera)
    {
        Camera = World->GetMainCameraComponent();
    }

    if (!Camera)
    {
        return false;
    }

    return SetViewTarget(Camera);
}

UCameraComponent* APlayerCameraManager::GetViewTargetCamera() const
{
    if (!ViewTarget.IsValidForWorld(GetWorld()))
    {
        return nullptr;
    }
    return ViewTarget.TargetCamera;
}

bool APlayerCameraManager::GetCameraPOV(FCameraPOV& OutPOV) const
{
    UCameraComponent* Camera = GetViewTargetCamera();
    if (!Camera)
    {
        return false;
    }

    OutPOV = Camera->GetCameraPOV();

    return OutPOV.IsValid();
}
