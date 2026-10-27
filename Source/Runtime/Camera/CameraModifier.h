#pragma once

#include "CameraViewInfo.h"

class FCameraModifier
{
public:
	virtual ~FCameraModifier() = default;

	void SetPriority(int32 InPriority) { Priority = InPriority; }
	int32 GetPriority() const { return Priority; }
	void SetEnabled(bool bEnabled);
	bool IsEnabled() const { return bIsEnabled; }
	void SetBlendTimes(float InAlphaInDuration, float InAlphaOutDuration);
	void Activate();
	void Deactivate();
	float GetAlpha() const { return Alpha; }
	void ApplyCamera(float DeltaTime, FCameraPOV& InOutPOV);
	virtual void ModifyCamera(float DeltaTime, FCameraPOV& InOutPOV) = 0;
	virtual bool IsFinished() const { return false; }
private:
	int32 Priority = 0;

	bool bIsEnabled = true;
	bool bIsBlendingOut = false;
	bool bPendingRemoval = false;

	float Alpha = 1.0f;

	float AlphaInDuration = 0.0f;
	float AlphaOutDuration = 0.0f;

	float BlendElapsed = 0.0f;
	float BlendOutStartAlpha = 1.0f;
};

class FCameraShakeModifier final : public FCameraModifier
{
public:
	void Start(float InDuration, float InLocationAmplitude, float InRotationAmplitude, float InFrequency);
	void ModifyCamera(float DeltaTime, FCameraPOV& InOutPOV) override;
	bool IsFinished() const override { return !bIsActive || FCameraModifier::IsFinished(); }

private:
	bool bIsActive = false;

	float Duration = 0.0f;
	float ElapsedTime = 0.0f;

	float LocationAmplitude = 0.0f;
	float RotationAmplitude = 0.0f;
	float Frequency = 0.0f;
};