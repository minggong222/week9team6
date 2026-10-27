#include "EnginePCH.h"
#include "CameraModifier.h"

#include <algorithm>
#include <cmath>

namespace
{
	constexpr float TwoPi = 6.2831853071795864769f;
}

void FCameraShakeModifier::Start(float InDuration, float InLocationAmplitude, float InRotationAmplitude, float InFrequency)
{
	bIsActive = false;
	Duration = 0.0f;
	ElapsedTime = 0.0f;
    if (!std::isfinite(InDuration) || !std::isfinite(InLocationAmplitude) || !std::isfinite(InRotationAmplitude) || !std::isfinite(InFrequency) || InDuration <= 0.0f || InFrequency <= 0.0f)
    {
        return;
    }

	Duration = InDuration;
	LocationAmplitude = InLocationAmplitude;
	RotationAmplitude = InRotationAmplitude;
	Frequency = InFrequency;

	bIsActive = LocationAmplitude != 0.0f || RotationAmplitude != 0.0f;
}

void FCameraShakeModifier::ModifyCamera(float DeltaTime, FCameraPOV& InOutPOV)
{
	if (!bIsActive || !std::isfinite(DeltaTime) || DeltaTime <= 0.0f)
	{
		return;
	}
	ElapsedTime = std::min(Duration, ElapsedTime + DeltaTime);

	const float Fade = std::max(0.0f, 1.0f - ElapsedTime / Duration);
	const float Phase = ElapsedTime * Frequency * TwoPi;
	const float LocationOffset = std::sin(Phase) * LocationAmplitude * Fade;
	const float RotationOffset = std::cos(Phase) * RotationAmplitude * Fade;
	const FVector LocalOffset(0.0f, 0.0f, LocationOffset);

	InOutPOV.Location += InOutPOV.Rotation.Quaternion().RotateVector(LocalOffset);
	InOutPOV.Rotation.Roll += RotationOffset;

	if (ElapsedTime >= Duration)
	{
		bIsActive = false;
	}
}
