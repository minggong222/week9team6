#include "EnginePCH.h"
#include "CameraZoomModifier.h"

#include "Math/MathUtility.h"

#include <algorithm>
#include <cmath>

void FCameraZoomModifier::Start(float InScale, float InZoomInDuration, float InZoomOutDuration)
{
	bIsActive = false;
	ElapsedTime = 0.0f;
	if (!std::isfinite(InScale) || InScale <= 0.0f
		|| !std::isfinite(InZoomInDuration) || InZoomInDuration < 0.0f
		|| !std::isfinite(InZoomOutDuration) || InZoomOutDuration <= 0.0f
		|| !std::isfinite(InZoomInDuration + InZoomOutDuration))
	{
		return;
	}

	Scale = InScale;
	ZoomInDuration = InZoomInDuration;
	ZoomOutDuration = InZoomOutDuration;
	Activate();
	bIsActive = true;
}

void FCameraZoomModifier::ModifyCamera(float DeltaTime, FCameraPOV& InOutPOV)
{
	if (!bIsActive)
	{
		return;
	}

	ElapsedTime = std::min(ZoomInDuration + ZoomOutDuration, ElapsedTime + DeltaTime);
	if (ElapsedTime >= ZoomInDuration + ZoomOutDuration)
	{
		bIsActive = false;
		return;
	}

	const float Alpha = ElapsedTime < ZoomInDuration
		? ElapsedTime / ZoomInDuration
		: 1.0f - (ElapsedTime - ZoomInDuration) / ZoomOutDuration;
	const float SmoothAlpha = Alpha * Alpha * (3.0f - 2.0f * Alpha);
	const float CurrentScale = 1.0f + (Scale - 1.0f) * SmoothAlpha;

	// 컴포넌트 원본은 유지하고 최종 POV만 변경하므로 종료 시 별도 복원이 필요 없다.
	InOutPOV.OrthoWidth *= CurrentScale;
	const float HalfFOV = InOutPOV.FieldOfView * PI / 360.0f;
	InOutPOV.FieldOfView = std::clamp(std::atan(std::tan(HalfFOV) * CurrentScale) * 360.0f / PI, 1.0f, 179.0f);
}
