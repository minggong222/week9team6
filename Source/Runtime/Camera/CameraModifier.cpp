#include "EnginePCH.h"
#include "CameraModifier.h"

#include "Math/MathUtility.h"

#include <algorithm>
#include <cmath>

namespace
{

	float InterpolateAngle(float From, float To, float Alpha)
	{
		const float Delta = std::remainder(To - From, 360.0f);
		return From + Delta * Alpha;
	}

	FCameraPOV BlendModifierPOV(const FCameraPOV& From, const FCameraPOV& To, float Alpha)
	{
		Alpha = std::clamp(Alpha, 0.0f, 1.0f);

		FCameraPOV Result;

		Result.Location = From.Location + (To.Location - From.Location) * Alpha;

		Result.Rotation = FRotator(InterpolateAngle(From.Rotation.Pitch, To.Rotation.Pitch, Alpha), InterpolateAngle(From.Rotation.Yaw, To.Rotation.Yaw, Alpha), InterpolateAngle(From.Rotation.Roll, To.Rotation.Roll, Alpha));

		Result.FieldOfView = From.FieldOfView + (To.FieldOfView - From.FieldOfView) * Alpha;

		Result.AspectRatio = From.AspectRatio + (To.AspectRatio - From.AspectRatio) * Alpha;

		Result.NearClip = From.NearClip + (To.NearClip - From.NearClip) * Alpha;

		Result.FarClip = From.FarClip + (To.FarClip - From.FarClip) * Alpha;

		Result.OrthoWidth = From.OrthoWidth + (To.OrthoWidth - From.OrthoWidth) * Alpha;

		Result.PerspectiveBlend = From.PerspectiveBlend + (To.PerspectiveBlend - From.PerspectiveBlend) * Alpha;

		Result.bIsPerspective = Result.PerspectiveBlend >= 0.5f;

		return Result;
	}
}

void FCameraModifier::SetEnabled(bool bEnabled)
{
	bIsEnabled = bEnabled;
	bIsBlendingOut = false;
	BlendElapsed = 0.0f;
	if (bEnabled)
	{
		Alpha = 1.0f;
		bPendingRemoval = false;
	}
	else
	{
		Alpha = 0.0f;
	}
}

void FCameraModifier::SetBlendTimes(float InAlphaInDuration, float InAlphaOutDuration)
{
	AlphaInDuration = std::isfinite(InAlphaInDuration) && InAlphaInDuration > 0.0f ? InAlphaInDuration : 0.0f;

	AlphaOutDuration = std::isfinite(InAlphaOutDuration) && InAlphaOutDuration > 0.0f ? InAlphaOutDuration : 0.0f;
}

void FCameraModifier::Activate()
{
	bIsEnabled = true;
	bIsBlendingOut = false;
	bPendingRemoval = false;

	BlendElapsed = 0.0f;
	BlendOutStartAlpha = 1.0f;

	Alpha = AlphaInDuration > 0.0f ? 0.0f : 1.0f;
}

void FCameraModifier::Deactivate()
{
	if (!bIsEnabled || bPendingRemoval)
	{
		return;
	}

	if (AlphaOutDuration <= 0.0f || Alpha <= 0.0f)
	{
		bIsEnabled = false;
		bIsBlendingOut = false;
		Alpha = 0.0f;
		bPendingRemoval = true;
		return;
	}

	bIsBlendingOut = true;
	BlendElapsed = 0.0f;
	BlendOutStartAlpha = Alpha;
}

void FCameraModifier::ApplyCamera(float DeltaTime, FCameraPOV& InOutPOV)
{
	if (!bIsEnabled || bPendingRemoval)
	{
		return;
	}
	const float SafeDeltaTime = std::isfinite(DeltaTime) && DeltaTime > 0.0f ? DeltaTime : 0.0f;

	if (bIsBlendingOut)
	{
		BlendElapsed += SafeDeltaTime;
		const float Progress = std::clamp(BlendElapsed / AlphaOutDuration, 0.0f, 1.0f);
		Alpha = BlendOutStartAlpha * (1.0f - Progress);
		if (Progress >= 1.0f)
		{
			Alpha = 0.0f;
			bIsBlendingOut = false;
			bIsEnabled = false;
			bPendingRemoval = true;
			return;
		}
	}
	else if (Alpha < 1.0f)
	{
		if (AlphaInDuration <= 0.0f)
		{
			Alpha = 1.0f;
		}
		else
		{
			Alpha = std::clamp(Alpha + SafeDeltaTime / AlphaInDuration, 0.0f, 1.0f);
		}
	}
	
	if (Alpha <= 0.0f)
	{
		return;
	}

	const FCameraPOV OriginalPOV = InOutPOV;
	FCameraPOV ModifiedPOV = OriginalPOV;
	ModifyCamera(SafeDeltaTime, ModifiedPOV);
	InOutPOV = BlendModifierPOV(OriginalPOV, ModifiedPOV, Alpha);
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

	Activate();

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
	const float Phase = ElapsedTime * Frequency * TWO_PI;
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
