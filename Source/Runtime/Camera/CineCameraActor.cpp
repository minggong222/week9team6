#include "EnginePCH.h"
#include "CineCameraActor.h"
#include <cmath>

ACineCameraActor::ACineCameraActor()
{
	UpdateCameraFOV();
}
void ACineCameraActor::SetSensorWidth(float InSensorWidth)
{
	if (!std::isfinite(InSensorWidth) || InSensorWidth <= 0.0f)
	{
		return;
	}

	SensorWidth = InSensorWidth;
	UpdateCameraFOV();
}

void ACineCameraActor::SetFocalLength(float InFocalLength)
{
	if (!std::isfinite(InFocalLength) || InFocalLength <= 0.0f)
	{
		return;
	}

	FocalLength = InFocalLength;
	UpdateCameraFOV();
}

void ACineCameraActor::UpdateCameraFOV()
{
	UCameraComponent* Camera = GetCameraComponent();
	if (!Camera || !std::isfinite(SensorWidth) || !std::isfinite(FocalLength) || SensorWidth <= 0.0f || FocalLength <= 0.0f)
	{
		return;
	}
	const float AspectRatio = Camera->GetAspectRatio();
	if (!std::isfinite(AspectRatio) || AspectRatio <= 0.0f)
	{
		return;
	}
	const float HalfFOVRadians = std::atan(SensorWidth / (2.0f * FocalLength * AspectRatio));
	float VerticalFOV = 2.0f * HalfFOVRadians * 180.0f / PI;
	VerticalFOV = std::clamp(VerticalFOV, 1.0f, 179.0f);
	Camera->SetFieldOfView(VerticalFOV);
}

void ACineCameraActor::PostEditChangeProperty(const FProperty& Property)
{
	Super::PostEditChangeProperty(Property);
	if (Property.Name == "SensorWidth" || Property.Name == "FocalLength")
	{
		UpdateCameraFOV();
	}
}
