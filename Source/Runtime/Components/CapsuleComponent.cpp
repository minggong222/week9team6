#include "EnginePCH.h"
#include "CapsuleComponent.h"
#include "Render/LineBatcher.h"

void UCapsuleComponent::DrawDebug(FLineBatcher* LineBatcher, const FVector4& Color) const
{
    const FVector Axis = GetWorldRotation().Quaternion().RotateVector(FVector(0.0f, 0.0f, 1.0f));
    LineBatcher->AddWireCapsule(GetWorldLocation(), Axis, GetScaledCapsuleRadius(), CapsuleHalfHeight, Color);
}