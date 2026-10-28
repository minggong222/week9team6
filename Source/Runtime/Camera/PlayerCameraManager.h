#pragma once

#include "GameFramework/Actor.h"
#include "CameraViewInfo.h"
#include "Math/Color.h"
#include "Camera/CameraModifier.h"

#include <memory>
#include <vector>
class APlayerController;
class UCameraComponent;
class UWorld;
class FCameraZoomModifier;


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
	bool EvaluateCamera(float DeltaTime, float ModifierDeltaTime = -1.0f);

	// 투명도 0은 원래 화면, 1은 지정 색상. 완료 후 끝 값을 유지한다.
	bool StartCameraFade(float FromAlpha, float ToAlpha, float Duration, const FColor& Color);
	void StopCameraFade();
	float GetFadeAmount() const { return FadeAmount; }
	const FColor& GetFadeColor() const { return FadeColor; }

	// 상하 띠 각각의 높이를 화면 높이의 0~0.5 비율로 지정한다.
	bool SetLetterBox(float Height, float Duration);
	void StopLetterBox();
	float GetLetterBoxHeight() const { return LetterBoxHeight; }

	FCameraModifier* AddCameraModifier(TUniquePtr<FCameraModifier> Modifier);
	bool RemoveCameraModifier(FCameraModifier* Modifier);
	void ClearCameraModifiers();
	// Scale < 1은 줌인, Scale > 1은 줌아웃 후 원래 배율로 복귀한다. 실제 시간 기준.
	bool StartCameraZoom(float Scale, float ZoomInDuration, float ZoomOutDuration, int32 Priority = 0);
	void StopCameraZoom();
	FCameraShakeModifier* StartCameraShake(float Duration, float LocationAmplitude, float RotationAmplitude, float Frequency, int32 Priority = 0);
private:
	void UpdateCameraEffects(float DeltaTime);

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

	FColor FadeColor = FColor(0, 0, 0, 255);
	float FadeAmount = 0.0f;
	float FadeFrom = 0.0f;
	float FadeTarget = 0.0f;
	float FadeDuration = 0.0f;
	float FadeElapsed = 0.0f;

	float LetterBoxHeight = 0.0f;
	float LetterBoxFrom = 0.0f;
	float LetterBoxTarget = 0.0f;
	float LetterBoxDuration = 0.0f;
	float LetterBoxElapsed = 0.0f;

	std::vector<TUniquePtr<FCameraModifier>> CameraModifiers;
	FCameraZoomModifier* ActiveZoomModifier = nullptr;
};
