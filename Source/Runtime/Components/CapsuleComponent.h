#pragma once

#include "ShapeComponent.h"

#include <algorithm>

class UCapsuleComponent : public UShapeComponent
{
	DECLARE_CLASS(UCapsuleComponent, UShapeComponent)

	REFLECT_START(ClassName)
		PROPERTY(CapsuleRadius)
		PROPERTY(CapsuleHalfHeight)
	REFLECT_END()

public:

	float GetUnscaledCapsuleRadius() const { return CapsuleRadius; }
	float GetUnscaledCapsuleHalfHeight() const { return CapsuleHalfHeight; }

	void SetCapsuleSize(float InRadius, float InHalfHeight)
	{
		CapsuleRadius = std::max(0.001f, InRadius);
		CapsuleHalfHeight = std::max(CapsuleRadius, InHalfHeight);
		MarkCollisionBoundsDirty();
	}

	float GetShapeScale() const
	{
		const FVector Scale = GetWorldScale3D().GetAbs();
		return std::min({ Scale.X, Scale.Y, Scale.Z });
	}

	float GetScaledCapsuleRadius() const
	{
		return CapsuleRadius * GetShapeScale();
	}

	float GetScaledCapsuleHalfHeight() const
	{
		return CapsuleHalfHeight * GetShapeScale();
	}

	FBox CalcLocalBounds() const override
	{
		const FVector Extent(CapsuleRadius, CapsuleRadius, CapsuleHalfHeight);
		return FBox{ -Extent, Extent };
	}

	virtual void DrawDebug(FLineBatcher* LineBatcher, const FVector4& Color) const override;

private:
	// 스케일 적용 전 캡슐의 반지름과 전체 높이의 절반
	float CapsuleRadius = 0.5f;
	float CapsuleHalfHeight = 1.0f;
};
