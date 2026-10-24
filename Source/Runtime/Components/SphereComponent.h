#pragma once

#include "ShapeComponent.h"

#include <algorithm>

class USphereComponent : public UShapeComponent
{
	DECLARE_CLASS(USphereComponent, UShapeComponent)

	REFLECT_START(ClassName)
		PROPERTY(SphereRadius)
	REFLECT_END()

public:

	float GetUnscaledSphereRadius() const { return SphereRadius; }

	void SetSphereRadius(float InRadius)
	{
		SphereRadius = std::max(0.0f, InRadius);
	}

	float GetScaledSphereRadius() const
	{
		const FVector Scale = GetWorldScale3D().GetAbs();
		return SphereRadius * std::min({ Scale.X, Scale.Y, Scale.Z });
	}

	FBox CalcLocalBounds() const override
	{
		const FVector Extent(SphereRadius);
		return FBox{ -Extent, Extent };
	}

	virtual void DrawDebug(FLineBatcher* LineBatcher, const FVector4& Color) const override;

private:
	float SphereRadius = 0.5f;
};
