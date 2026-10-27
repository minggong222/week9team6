#include "EnginePCH.h"
#include "PlayerCameraManager.h"

#include "Camera/CameraComponent.h"
#include "Camera/CameraActor.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Components/ActorComponent.h"
#include "Camera/SpringArmComponent.h"

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

    return false;
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

    EvaluateCamera(0.0f);

    return true;
}

void APlayerCameraManager::BeginPlay()
{
    Super::BeginPlay();
    EvaluateCamera(0.0f);
}

void APlayerCameraManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    EvaluateCamera(DeltaTime);
}

void APlayerCameraManager::EndPlay()
{
    ViewTarget.Reset();
    FinalPOV = FCameraPOV{};
    bHasFinalPOV = false;

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

    return EvaluateCamera(0.0f);
}

void APlayerCameraManager::ResetViewTarget()
{
    ViewTarget.Reset();
    FinalPOV = FCameraPOV{};
    bHasFinalPOV = false;
    
    EvaluateCamera(0.0f);
}

bool APlayerCameraManager::RefreshViewTarget()
{
    UWorld* World = GetWorld();

    if (!World)
    {
        ViewTarget.Reset();
        bHasFinalPOV = false;
        return false;
    }

    if (ViewTarget.IsValidForWorld(World))
    {
        return true;
    }

    ViewTarget.Reset();
    bHasFinalPOV = false;

    // 1순위 플레이어 카메라 
    UCameraComponent* Camera = World->GetPlayerCamera();

    if (Camera)
    {
        FViewTarget NewTarget;
        if (NewTarget.SetTarget(Camera) && NewTarget.IsValidForWorld(World))
        {
            ViewTarget = NewTarget;
            return true;
        }
    }


    // 2순위 기존 World MainCamera
    Camera = World->GetMainCameraComponent();

    if (Camera)
    {
        FViewTarget NewTarget;
        if (NewTarget.SetTarget(Camera) && NewTarget.IsValidForWorld(World))
        {
            ViewTarget = NewTarget;
            return true;
        }
    }

    return false;
}

UCameraComponent* APlayerCameraManager::GetViewTargetCamera() const
{
    if (!ViewTarget.IsValidForWorld(GetWorld()))
    {
        return nullptr;
    }
    return ViewTarget.TargetCamera;
}

bool APlayerCameraManager::GetCameraPOV(FCameraPOV& OutPOV)
{
    if (!EvaluateCamera(0.0f))
    {
        return false;
    }

    OutPOV = FinalPOV;

    return OutPOV.IsValid();
}

bool APlayerCameraManager::EvaluateCamera(float DeltaTime)
{
    // 아직 시간 기반 전환 구현 x
    // 이후 Transition 평가에서 DeltaTime 사용
    static_cast<void>(DeltaTime);

    if (!RefreshViewTarget())
    {
        FinalPOV = FCameraPOV{};
        bHasFinalPOV = false;
        return false;
    }

    UCameraComponent* Camera = ViewTarget.TargetCamera;

    if (!Camera)
    {
        FinalPOV = FCameraPOV{};
        bHasFinalPOV = false;
        return false;
    }

    if (USpringArmComponent* SpringArm = Cast<USpringArmComponent>(Camera->GetAttachParent()))
    {
        SpringArm->UpdateSpringArm(DeltaTime, Camera);
    }

    // 1. 기본 카메라 상태
    FCameraPOV EvaluatedPOV = Camera->GetCameraPOV();
    if (!EvaluatedPOV.IsValid())
    {
        bHasFinalPOV = false;
        return false;
    }
    // 2. Spring Arm 평가
    // 
    // 3. Camera Transition 평가
    // 4. Camera Modifier 적용

    // 5. 최종 카메라 상태 확정
    ViewTarget.POV = EvaluatedPOV;
    FinalPOV = EvaluatedPOV;
    bHasFinalPOV = FinalPOV.IsValid();

    return bHasFinalPOV;
}
