#include "EnginePCH.h"
#include "SphereComponent.h"
#include "Render/LineBatcher.h"

void USphereComponent::DrawDebug(FLineBatcher* LineBatcher, const FVector4& Color) const
{
    LineBatcher->AddWireSphere(GetWorldLocation(), SphereRadius, Color);
}