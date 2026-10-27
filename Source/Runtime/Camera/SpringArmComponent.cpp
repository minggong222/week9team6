#include "EnginePCH.h"
#include "SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

#include <algorithm>
#include <cmath>

namespace
{
    float GetLagAlpha(float Speed, float DeltaTime)
    {
        if (Speed <= 0.0f || DeltaTime <= 0.0f)
        {
            return 0.0f;
        }
        return 1.0f - std::exp(-Speed * DeltaTime);
    }

    float InterpAngle(float Current, float Target, float Alpha)
    {
        const float Delta = std::remainder(Target - Current, 360.0f);

        return Current + Delta * Alpha;
    }

    FRotator InterpRotation(const FRotator& Current, const FRotator& Target, float Alpha)
    {
        return FRotator(InterpAngle(Current.Pitch, Target.Pitch, Alpha), InterpAngle(Current.Yaw, Target.Yaw, Alpha), InterpAngle(Current.Roll, Target.Roll, Alpha));
    }

}

USpringArmComponent::USpringArmComponent()
{
    // 갱신은 PlayerCameraManager가 카메라 평가 중 담당
    // 별도의 Component Tick은 등록하지 않음
}

bool USpringArmComponent::UpdateSpringArm(float DeltaTime, UCameraComponent* Camera)
{
    if (!Camera || Camera->GetAttachParent() != this || Camera->GetWorld() != GetWorld())
    {
        return false;
    }

    if (!std::isfinite(DeltaTime) || DeltaTime < 0.0f)
    {
        DeltaTime = 0.0f;
    }

    if (CachedCamera != Camera)
    {
        CachedCamera = Camera;
        CameraRotationOffset = Camera->GetRelativeRotation();
        bHasCameraState = false;
    }

    const FQuat ArmRotation = GetWorldRotation().Quaternion();

    const FVector Pivot = GetWorldLocation() + ArmRotation.RotateVector(TargetOffset);

    const float ArmLength = std::max(0.0f, TargetArmLength);

    const FVector DesiredOffset = SocketOffset + FVector(-ArmLength, 0.0f, 0.0f);

    const FVector DesiredLocation = Pivot + ArmRotation.RotateVector(DesiredOffset);

    const FVector CollisionLimitedLocation = ResolveCameraCollision(Pivot, DesiredLocation);

    const FVector CurrentCameraLocation = Camera->GetWorldLocation();

    FVector FinalLocation;

    if (!bHasCameraState)
    {
        FinalLocation = CollisionLimitedLocation;
    }
    else if (bEnableCameraLag && DeltaTime > 0.0f)
    {
        const float Alpha = GetLagAlpha(CameraLagSpeed, DeltaTime);

        FinalLocation = CurrentCameraLocation + (CollisionLimitedLocation - CurrentCameraLocation) * Alpha;
    }
    else
    {
        FinalLocation = CollisionLimitedLocation;
    }

    FinalLocation = ResolveCameraCollision(Pivot, FinalLocation);

    const FQuat TargetCameraQuat = ArmRotation * CameraRotationOffset.Quaternion();

    const FRotator TargetCameraRotation = TargetCameraQuat.ToFRotator();

    const FRotator CurrentCameraRotation = Camera->GetWorldRotation();

    FRotator FinalRotation;

    if (!bHasCameraState)
    {
        FinalRotation = TargetCameraRotation;
    }
    else if (bEnableCameraRotationLag && DeltaTime > 0.0f)
    {
        const float Alpha = GetLagAlpha(CameraRotationLagSpeed, DeltaTime);
        FinalRotation = InterpRotation(CurrentCameraRotation, TargetCameraRotation, Alpha);
    }
    else
    {
        FinalRotation = TargetCameraRotation;
    }

    Camera->SetWorldLocationAndRotation(FinalLocation, FinalRotation, false);
    bHasCameraState = true;
    return true;
}

FVector USpringArmComponent::ResolveCameraCollision(const FVector& Pivot, const FVector& DesiredLocation) const
{
    if (!bDoCollisionTest)
    {
        return DesiredLocation;
    }

    UWorld* World = GetWorld();
    AActor* Owner = GetOwner();

    if (!World || !Owner)
    {
        return DesiredLocation;
    }

    const FVector Delta = DesiredLocation - Pivot;
    const float Distance = Delta.Size();

    if (Distance <= EPSILON_1E_8)
    {
        return DesiredLocation;
    }

    const FRay Ray{ Pivot, Delta / Distance };
    FHitResult Hit;

    if (!World->SweepCameraSphere(Ray, Distance, std::max(0.0f, CollisionProbeSize), Hit, Owner))
    {
        return DesiredLocation;
    }

    const float SafeDistance = std::max(0.0f, Hit.Distance - std::max(0.0f, CollisionSafetyMargin));

    return Pivot + Ray.Direction * SafeDistance;
}
