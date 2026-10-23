#include "EnginePCH.h"
#include "PointLightComponent.h"

#include "Engine/LightSceneProxy.h"
#include "Render/LineBatcher.h"
#include "Math/Color.h"

UPointLightComponent::UPointLightComponent() : LightFalloffExponent(1.0f) {}
UPointLightComponent::~UPointLightComponent() {}

FLightSceneProxy* UPointLightComponent::CreateSceneProxy() const
{
    return new FPointLightSceneProxy(this);
}

void UPointLightComponent::DrawDebug(FLineBatcher* LineBatcher) const
{
    if (LineBatcher == nullptr)
        return;

    const FVector Center = GetWorldLocation();
    const float Radius = std::max(0.0f, GetAttenuationRadius());
    const FVector4 Color = FColor(0, 248, 255, 255).ToVector4();

    LineBatcher->AddWireSphere(Center, Radius, Color);
}