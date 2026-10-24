#pragma once

#include "ShapeComponent.h"

#include <algorithm>

class FLineBatcher;

class UBoxComponent : public UShapeComponent
{
	DECLARE_CLASS(UBoxComponent, UShapeComponent)

	REFLECT_START(ClassName)
		PROPERTY(BoxExtent)
	REFLECT_END()

public:

	FVector GetUnscaledBoxExtent() const { return BoxExtent; }

	void SetBoxExtent(const FVector& InExtent) 
	{ 
		BoxExtent.X = std::max(0.0f, InExtent.X); 
		BoxExtent.Y = std::max(0.0f, InExtent.Y);
		BoxExtent.Z = std::max(0.0f, InExtent.Z);
		MarkCollisionBoundsDirty();
	}

	FVector GetScaledBoxExtent() const { return BoxExtent * GetWorldScale3D().GetAbs(); }

	FBox CalcLocalBounds() const { return FBox{ -BoxExtent, BoxExtent }; }
	virtual void DrawDebug(FLineBatcher* LineBatcher, const FVector4& Color) const override;
private:

	// 상자 중심에서 각 면까지의 거리(반거리)
	FVector BoxExtent = FVector(0.5f, 0.5f, 0.5f);
};