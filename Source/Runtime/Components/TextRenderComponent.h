#pragma once

#include "BillboardComponent.h"
#include "Math/Color.h"
#include "Text/Font.h"
#include "Render/StaticMeshData.h"

class UTextRenderComponent : public UPrimitiveComponent
{
	DECLARE_CLASS(UTextRenderComponent, UPrimitiveComponent)

	REFLECT_START(ClassName)
		PROPERTY(Text)
		PROPERTY(TextSize)
		PROPERTY_TYPE(TextColor, Color)
		PROPERTY(Font)
		REFLECT_END()

public:
	UTextRenderComponent();
	virtual ~UTextRenderComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime) override;

	virtual const FStaticMeshData* GetMeshData() const override;


	// 텍스트는 별도 렌더러에서 처리하므로 렌더 패킷을 생성하지 않음
	virtual void SubmitToRenderQueue(FRenderQueue& RenderQueue, const FViewContext& ViewContext) override {}

	const FString& GetText() const { return Text; }
	void SetText(const FString& InText) { Text = InText; }

	UFont* GetFont() const { return Font; }
	void SetFont(UFont* InFont) { Font = InFont; }

	float GetTextSize() const { return TextSize < 0 ? 0.1f : TextSize; }
	void SetTextSize(float InTextSize) { TextSize = InTextSize; }
    const FVector4& GetTextColor() const { return TextColor; }
    void SetTextColor(const FColor& Color) { TextColor = FVector4(Color.R / 255.0f, Color.G / 255.0f, Color.B / 255.0f, Color.A / 255.0f); }

private:
	bool RebuildPickingMesh() const;

	FString Text = "Text";
	UFont* Font = nullptr;
	float TextSize = 1.0f;
    FVector4 TextColor = FVector4(1, 1, 1, 1);

	mutable FStaticMeshData PickingMesh;
	mutable FString CachedText;
	mutable float CachedTextSize = -1.0f;
	mutable UFont* CachedFont = nullptr;
	mutable bool bHasPickingMesh = false;
};
