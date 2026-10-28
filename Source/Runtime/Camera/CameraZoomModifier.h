#pragma once

#include "CameraModifier.h"

// 투영 폭을 일시적으로 변경한 뒤 원래 배율로 복귀하는 줌 연출.
class FCameraZoomModifier final : public FCameraModifier
{
public:
	void Start(float InScale, float InZoomInDuration, float InZoomOutDuration);
	void ModifyCamera(float DeltaTime, FCameraPOV& InOutPOV) override;
	bool IsFinished() const override { return !bIsActive || FCameraModifier::IsFinished(); }

private:
	bool bIsActive = false;
	float Scale = 1.0f;
	float ZoomInDuration = 0.0f;
	float ZoomOutDuration = 0.0f;
	float ElapsedTime = 0.0f;
};
