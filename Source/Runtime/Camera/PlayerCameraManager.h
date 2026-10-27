#pragma once

#include "GameFramework/Actor.h"
#include "CameraViewInfo.h"

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

	bool SetViewTarget(UCameraComponent* InCamera);
	void ResetViewTarget();
	bool RefreshViewTarget();
	UCameraComponent* GetViewTargetCamera() const;
	bool GetCameraPOV(FCameraPOV& OutPOV) const;
	const FViewTarget& GetViewTarget() const { return ViewTarget; }
	APlayerController* GetPlayerController() const { return PlayerController; }
	bool EvaluateCamera(float DeltaTime);
private:
	APlayerController* PlayerController = nullptr;
	FViewTarget ViewTarget;

	FCameraPOV FinalPOV;
	bool bHasFinalPOV = false;
};