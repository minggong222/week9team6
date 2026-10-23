#include "EnginePCH.h"
#include "BoxComponent.h"
#include "Render/LineBatcher.h"

void UBoxComponent::DrawDebug(FLineBatcher* LineBatcher, const FVector4& Color) const
{
    LineBatcher->AddOrientedBox(CalcLocalBounds(), GetWorldMatrix(), Color);
}