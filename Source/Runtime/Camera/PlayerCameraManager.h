#pragma once

#include "GameFramework/Actor.h"
#include "CameraViewInfo.h"
#include "Camera/CameraModifier.h"

#include <memory>
#include <vector>
class APlayerController;
class UCameraComponent;
class UWorld;


// 현재 카메라 시점을 제공하는 대상
// 대상 Actor와 Component의 UUID를 함께 보관해 파괴되거나 교체된 객체를 검증할 수 있게 함
struct FViewTarget
{
	AActor* TargetActor = nullptr;
	UCameraComponent* TargetCamera = nullptr;

	uint32 TargetActorUUID = 0;
	uint32 TargetCameraUUID = 0;

	// 현재 대상의 카메라 상태 스냅샷
	FCameraPOV POV;

	bool SetTarget(UCameraComponent* InCamera);
	bool IsValidForWorld(const UWorld* InWorld) const;
	void Reset();
};

class APlayerCameraManager : public AActor
{
	DECLARE_CLASS(APlayerCameraManager, AActor)
	REFLECT_START(ClassName)
	REFLECT_END()
public:
	APlayerCameraManager();

	bool InitalizeForPlayer(APlayerController* InPlayerController);

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay() override;

	bool SetViewTarget(UCameraComponent* InCamera, float BlendTime = 0.0f);
	void ResetViewTarget();
	bool RefreshViewTarget();
	UCameraComponent* GetViewTargetCamera() const;
	bool GetCameraPOV(FCameraPOV& OutPOV) const;
	const FViewTarget& GetViewTarget() const { return ViewTarget; }
	APlayerController* GetPlayerController() const { return PlayerController; }
	bool EvaluateCamera(float DeltaTime);

	FCameraModifier* AddCameraModifier(TUniquePtr<FCameraModifier> Modifier);
	bool RemoveCameraModifier(FCameraModifier* Modifier);
	void ClearCameraModifiers();
	FCameraShakeModifier* StartCameraShake(float Duration, float LocationAmplitude, float RotationAmplitude, float Frequency);
private:
	APlayerController* PlayerController = nullptr;
	FViewTarget ViewTarget;

	FCameraPOV FinalPOV;
	bool bHasFinalPOV = false;

	// 전환이 시작될 때의 카메라 상태
	FCameraPOV TransitionStartPOV;

	// 전환 시간과 현재까지 경과한 시간
	float TransitionDuration = 0.0f;
	float TransitionElapsed = 0.0f;

	// 현재 카메라 전환이 진행 중인지 여부
	bool bIsTransitioning = false;

	std::vector<TUniquePtr<FCameraModifier>> CameraModifiers;
};