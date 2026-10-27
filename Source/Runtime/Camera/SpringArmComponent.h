#pragma once

#include "Components/SceneComponent.h"

class UCameraComponent;

class USpringArmComponent : public USceneComponent
{
	DECLARE_CLASS(USpringArmComponent, USceneComponent)
    REFLECT_START(ClassName)
        PROPERTY(TargetArmLength)
        PROPERTY(TargetOffset)
        PROPERTY(SocketOffset)
        PROPERTY(bDoCollisionTest)
        PROPERTY(CollisionProbeSize)
        PROPERTY(CollisionSafetyMargin)
        PROPERTY(bEnableCameraLag)
        PROPERTY(CameraLagSpeed)
        PROPERTY(bEnableCameraRotationLag)
        PROPERTY(CameraRotationLagSpeed)
    REFLECT_END()
public:
    USpringArmComponent();

    // PlayerCameraManager가 카메라 평가 중 호출한다.
    // Camera는 이 Spring Arm의 직접 자식이어야 한다.
    bool UpdateSpringArm(float DeltaTime, UCameraComponent* Camera);

    float GetTargetArmLength() const { return TargetArmLength; }
    void SetTargetArmLength(float InLength) { TargetArmLength = InLength > 0.0f ? InLength : 0.0f; }

    FVector GetTargetOffset() const { return TargetOffset; }
    void SetTargetOffset(const FVector& InOffset) { TargetOffset = InOffset; }

    FVector GetSocketOffset() const { return SocketOffset; }
    void SetSocketOffset(const FVector& InOffset) { SocketOffset = InOffset; }

private:
    FVector ResolveCameraCollision(const FVector& Pivot, const FVector& DesiredLocation) const;

    float TargetArmLength = 300.0f;

    // Spring Arm의 회전 기준에서 피벗과 카메라 위치를 보정한다.
    FVector TargetOffset = FVector(0.0f, 0.0f, 0.0f);
    FVector SocketOffset = FVector(0.0f, 0.0f, 0.0f);

    bool bDoCollisionTest = true;
    float CollisionProbeSize = 12.0f;
    float CollisionSafetyMargin = 2.0f;

    bool bEnableCameraLag = false;
    float CameraLagSpeed = 10.0f;

    bool bEnableCameraRotationLag = false;
    float CameraRotationLagSpeed = 10.0f;

    // 회전 지연을 위한 카메라의 최초 상대 회전을 보관한다.
    UCameraComponent* CachedCamera = nullptr;

    // 카메라의 초기 상대 회전을 보관
    FRotator CameraRotationOffset = FRotator(0.0f, 0.0f, 0.0f);

    // 부모의 이동·회전에 영향을 받지 않는 이전 프레임의 최종 카메라 상태
    FVector PreviousCameraLocation = FVector(0.0f, 0.0f, 0.0f);
    FRotator PreviousCameraRotation = FRotator(0.0f, 0.0f, 0.0f);

    bool bHasCameraState = false;
};