#include "EnginePCH.h"
#include "ShapeComponent.h"

UShapeComponent::UShapeComponent()
{
    bHiddenInGame = true;
    bCastShadow = false;
    bReceiveShadow = false;
}

//UShapeComponent::~UShapeComponent()
//{
//}
//
//void UShapeComponent::BeginPlay()
//{
//	Super::BeginPlay();
//}
//
//void UShapeComponent::TickComponent(float DeltaTime)
//{
//	Super::TickComponent(DeltaTime);
//}
