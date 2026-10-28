#pragma once

#include "Math/EngineMath.h"

// 전환 및 카메라 효과에서 사용하는 카메라 상태
// 렌더링용 FSceneView와 달리 보간 가능한 카메라 속성을 보관
struct FCameraPOV
{
	FVector Location = FVector(0.0f, 0.0f, 0.0f);
	FRotator Rotation = FRotator(0.0f, 0.0f, 0.0f);

	float FieldOfView = 60.0f;
	float AspectRatio = 16.0f / 9.0f;
	float NearClip = 0.1f;
	float FarClip = 10000.0f;
	float OrthoWidth = 16.0f;
	float ColorGradingGamma = 1.0f;

	bool bIsPerspective = true;
	float PerspectiveBlend = 1.0f;
	bool IsValid() const;
	
	FMatrix GetViewMatrix() const;
	FMatrix GetPerspectiveMatrix() const;
	FMatrix GetOrthogonalMatrix() const;
	FMatrix GetProjectionMatrix() const;
	FMatrix GetViewProjectionMatrix() const;
};