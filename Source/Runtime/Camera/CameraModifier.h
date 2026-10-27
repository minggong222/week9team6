#pragma once

#include "CameraViewInfo.h"

class FCameraModifier
{
public:
	virtual ~FCameraModifier() = default;

	void SetEnabled(bool bEnabled) { bIsEnabled = bEnabled; }
	bool IsEnabled() const { return bIsEnabled; }
	virtual void ModifyCamera(float DeltaTime, FCameraPOV& InOutPOV) = 0;
	virtual bool IsFinished() const { return false; }
private:
	bool bIsEnabled;
};

class FCameraShakeModifier final : public FCameraModifier
{
public:
	void Start(float InDuration, float InLocationAmplitude, float InRotationAmplitude, float InFrequency);
	void ModifyCamera(float DeltaTime, FCameraPOV& InOutPOV) override;
	bool IsFinished() const override { return !bIsActive; }

private:
	bool bIsActive = false;

	float Duration = 0.0f;
	float ElapsedTime = 0.0f;

	float LocationAmplitude = 0.0f;

	float RotationAmplitude = 0.0f;

	float Frequency = 0.0f;
};