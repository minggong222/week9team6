#pragma once

#include "PrimitiveComponent.h"

class FLineBatcher;

class UShapeComponent : public UPrimitiveComponent
{
	DECLARE_CLASS(UShapeComponent, UPrimitiveComponent)

	REFLECT_START(ClassName)
		PROPERTY(ShapeColor)
		PROPERTY(bDrawOnlyIfSelected)
	REFLECT_END()

public:
	UShapeComponent();

	const FColor& GetShapeColor() const { return ShapeColor; }
	bool GetDrawOnlyIfSelected() const { return bDrawOnlyIfSelected; }

	virtual void DrawDebug(FLineBatcher* LineBatcher, const FVector4& Color) const = 0;
	
protected: 
	bool ShouldCreateRenderState() const override { return false; }
	void OnRegister() override;
	void OnUnregister() override;
	void OnTransformDirty() override;
	void MarkCollisionBoundsDirty();

private: 
	FColor ShapeColor = FColor(223, 149, 157, 255);
	bool bDrawOnlyIfSelected = false;
};
