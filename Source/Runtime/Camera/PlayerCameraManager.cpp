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
    UpdateCameraEffects(DeltaTime);
    EvaluateCamera(DeltaTime);
}

void APlayerCameraManager::EndPlay()
{
    StopCameraFade();
    StopLetterBox();
    ViewTarget.Reset();
    FinalPOV = FCameraPOV{};
    bHasFinalPOV = false;
    bIsTransitioning = false;
    TransitionDuration = 0.0f;
    TransitionElapsed = 0.0f;
    TransitionStartPOV = FCameraPOV{};

    PlayerController = nullptr;
    ClearCameraModifiers();
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

    for (auto It = CameraModifiers.begin(); It != CameraModifiers.end();)
    {
        if (!(*It))
        {
            It = CameraModifiers.erase(It);
            continue;
        }
        FCameraModifier* Modifier = It->get();

        if (Modifier->IsEnabled())
        {
            Modifier->ModifyCamera(DeltaTime, FinalPOV);
        }

        if (Modifier->IsFinished())
        {
            It = CameraModifiers.erase(It);
            continue;
        }
        ++It;
    }

    bHasFinalPOV = FinalPOV.IsValid();

    return bHasFinalPOV;
}

bool APlayerCameraManager::StartCameraFade(float FromAlpha, float ToAlpha, float Duration, const FColor& Color)
{
    if (!std::isfinite(FromAlpha) || !std::isfinite(ToAlpha) || !std::isfinite(Duration) || Duration < 0.0f)
    {
        return false;
    }

    FadeColor = Color;
    FadeFrom = std::clamp(FromAlpha, 0.0f, 1.0f);
    FadeTarget = std::clamp(ToAlpha, 0.0f, 1.0f);
    FadeDuration = Duration;
    FadeElapsed = 0.0f;
    FadeAmount = Duration > 0.0f ? FadeFrom : FadeTarget;
    return true;
}

void APlayerCameraManager::StopCameraFade()
{
    FadeAmount = 0.0f;
    FadeFrom = 0.0f;
    FadeTarget = 0.0f;
    FadeDuration = 0.0f;
    FadeElapsed = 0.0f;
}

bool APlayerCameraManager::SetLetterBox(float Height, float Duration)
{
    if (!std::isfinite(Height) || !std::isfinite(Duration) || Duration < 0.0f)
    {
        return false;
    }

    LetterBoxFrom = LetterBoxHeight;
    LetterBoxTarget = std::clamp(Height, 0.0f, 0.5f);
    LetterBoxDuration = Duration;
    LetterBoxElapsed = 0.0f;
    if (Duration == 0.0f)
    {
        LetterBoxHeight = LetterBoxTarget;
    }
    return true;
}

void APlayerCameraManager::StopLetterBox()
{
    LetterBoxHeight = 0.0f;
    LetterBoxFrom = 0.0f;
    LetterBoxTarget = 0.0f;
    LetterBoxDuration = 0.0f;
    LetterBoxElapsed = 0.0f;
}

void APlayerCameraManager::UpdateCameraEffects(float DeltaTime)
{
    if (!std::isfinite(DeltaTime) || DeltaTime <= 0.0f)
    {
        return;
    }

    // POV 조회는 한 프레임에 여러 번 호출될 수 있으므로 효과 시간은 Tick에서만 진행한다.
    if (FadeDuration > 0.0f && FadeElapsed < FadeDuration)
    {
        FadeElapsed = std::min(FadeDuration, FadeElapsed + DeltaTime);
        const float Alpha = FadeElapsed / FadeDuration;
        FadeAmount = FadeFrom + (FadeTarget - FadeFrom) * Alpha;
    }

    if (LetterBoxDuration > 0.0f && LetterBoxElapsed < LetterBoxDuration)
    {
        LetterBoxElapsed = std::min(LetterBoxDuration, LetterBoxElapsed + DeltaTime);
        const float Alpha = LetterBoxElapsed / LetterBoxDuration;
        const float Blend = Alpha * Alpha * (3.0f - 2.0f * Alpha);
        LetterBoxHeight = LetterBoxFrom + (LetterBoxTarget - LetterBoxFrom) * Blend;
    }
}

FCameraModifier* APlayerCameraManager::AddCameraModifier(TUniquePtr<FCameraModifier> Modifier)
{
    if (!Modifier)
    {
        return nullptr;
    }

    FCameraModifier* Result = Modifier.get();
    CameraModifiers.emplace_back(std::move(Modifier));
    return Result;
}

bool APlayerCameraManager::RemoveCameraModifier(FCameraModifier* Modifier)
{
    if (!Modifier)
    {
        return false;
    }
    const auto It = std::find_if(CameraModifiers.begin(), CameraModifiers.end(), [Modifier](const TUniquePtr<FCameraModifier>& Entry)
        {
            return Entry.get() == Modifier;
        });
    if (It == CameraModifiers.end())
    {
        return false;
    }
    CameraModifiers.erase(It);
    return true;
}

void APlayerCameraManager::ClearCameraModifiers()
{
    CameraModifiers.clear();
}

FCameraShakeModifier* APlayerCameraManager::StartCameraShake(float Duration, float LocationAmplitude, float RotationAmplitude, float Frequency)
{
    auto Modifier = MakeUnique<FCameraShakeModifier>();
    Modifier->Start(Duration, LocationAmplitude, RotationAmplitude, Frequency);
    if (Modifier->IsFinished())
    {
        return nullptr;
    }
    FCameraShakeModifier* Result = Modifier.get();
    AddCameraModifier(std::move(Modifier));
    return Result;
}
