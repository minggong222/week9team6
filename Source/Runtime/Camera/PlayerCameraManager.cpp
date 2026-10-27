#include "EnginePCH.h"
#include "PlayerCameraManager.h"

#include "Camera/CameraComponent.h"
#include "Camera/CameraActor.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Components/ActorComponent.h"
#include "Camera/SpringArmComponent.h"

#include <algorithm>
#include <cmath>

namespace
{
    float InterpolateAngle(float Start, float Target, float Alpha)
    {
        const float Delta = std::remainder(Target - Start, 360.0f);
        return Start + Delta * Alpha;
    }

    FCameraPOV BlendCameraPOV(const FCameraPOV& Start, const FCameraPOV& Target, float Alpha)
    {
        FCameraPOV Result;

        Alpha = std::clamp(Alpha, 0.0f, 1.0f);

        Result.Location = Start.Location + (Target.Location - Start.Location) * Alpha;

        Result.Rotation = FRotator(InterpolateAngle(Start.Rotation.Pitch, Target.Rotation.Pitch, Alpha), InterpolateAngle(Start.Rotation.Yaw, Target.Rotation.Yaw, Alpha), InterpolateAngle(Start.Rotation.Roll, Target.Rotation.Roll, Alpha));
        
        Result.FieldOfView = Start.FieldOfView + (Target.FieldOfView - Start.FieldOfView) * Alpha;

        Result.AspectRatio = Start.AspectRatio + (Target.AspectRatio - Start.AspectRatio) * Alpha;

        Result.NearClip = Start.NearClip + (Target.NearClip - Start.NearClip) * Alpha;

        Result.FarClip = Start.FarClip + (Target.FarClip - Start.FarClip) * Alpha;

        Result.OrthoWidth = Start.OrthoWidth + (Target.OrthoWidth - Start.OrthoWidth) * Alpha;

        Result.bIsPerspective = Start.bIsPerspective;

        return Result;
    }
}

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
    bIsTransitioning = false;
    TransitionDuration = 0.0f;
    TransitionElapsed = 0.0f;
    TransitionStartPOV = FCameraPOV{};

    PlayerController = nullptr;

    Super::EndPlay();
}

bool APlayerCameraManager::SetViewTarget(UCameraComponent* InCamera, float BlendTime)
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

    const bool bCanBlend = std::isfinite(BlendTime) && BlendTime > 0.0f && bHasFinalPOV && FinalPOV.IsValid() && FinalPOV.bIsPerspective == NewTarget.POV.bIsPerspective;

    if (bCanBlend)
    {
        TransitionStartPOV = FinalPOV;

        TransitionDuration = BlendTime;
        TransitionElapsed = 0.0f;
        bIsTransitioning = true;
    }
    else
    {
        TransitionDuration = 0.0f;
        TransitionElapsed = 0.0f;
        bIsTransitioning = false;
    }

    ViewTarget = NewTarget;

    return EvaluateCamera(0.0f);
}

void APlayerCameraManager::ResetViewTarget()
{
    ViewTarget.Reset();
    FinalPOV = FCameraPOV{};
    bHasFinalPOV = false;
    bIsTransitioning = false;
    TransitionDuration = 0.0f;
    TransitionElapsed = 0.0f;
    TransitionStartPOV = FCameraPOV{};
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

bool APlayerCameraManager::GetCameraPOV(FCameraPOV& OutPOV) const
{
    if (!bHasFinalPOV || !FinalPOV.IsValid())
    {
        return false;
    }

    OutPOV = FinalPOV;
    return true;
}

bool APlayerCameraManager::EvaluateCamera(float DeltaTime)
{
    if (!RefreshViewTarget())
    {
        FinalPOV = FCameraPOV{};
        bHasFinalPOV = false;
        bIsTransitioning = false;
        return false;
    }

    UCameraComponent* Camera = ViewTarget.TargetCamera;

    if (!Camera)
    {
        FinalPOV = FCameraPOV{};
        bHasFinalPOV = false;
        bIsTransitioning = false;
        return false;
    }

    if (USpringArmComponent* SpringArm = Cast<USpringArmComponent>(Camera->GetAttachParent()))
    {
        SpringArm->UpdateSpringArm(DeltaTime, Camera);
    }

    const FCameraPOV TargetPOV = Camera->GetCameraPOV();

    if (!TargetPOV.IsValid())
    {
        FinalPOV = FCameraPOV{};
        bHasFinalPOV = false;
        bIsTransitioning = false;
        return false;
    }

    ViewTarget.POV = TargetPOV;

    if (bIsTransitioning && TransitionDuration > 0.0f)
    {
        const float SafeDeltaTime = (std::isfinite(DeltaTime) && DeltaTime > 0.0f) ? DeltaTime : 0.0f;
        TransitionElapsed = std::min(TransitionElapsed + SafeDeltaTime, TransitionDuration);
        const float Alpha = std::clamp(TransitionElapsed / TransitionDuration, 0.0f, 1.0f);

        FinalPOV = BlendCameraPOV(TransitionStartPOV, TargetPOV, Alpha);

        if (Alpha >= 1.0f)
        {
            FinalPOV = TargetPOV;
            bIsTransitioning = false;
        }
    }
    else
    {
        FinalPOV = TargetPOV;
    }

    bHasFinalPOV = FinalPOV.IsValid();

    return bHasFinalPOV;
}
