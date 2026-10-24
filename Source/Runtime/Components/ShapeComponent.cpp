#include "EnginePCH.h"
#include "ShapeComponent.h"

#include "Engine/World.h"

UShapeComponent::UShapeComponent()
{
    bHiddenInGame = true;
    bCastShadow = false;
    bReceiveShadow = false;
}

void UShapeComponent::OnRegister()
{
    Super::OnRegister();
    UWorld* World = GetWorld();
    World->GetCollisionScene().AddShape(this);
}

void UShapeComponent::OnUnregister()
{
    UWorld* World = GetWorld();
    World->GetCollisionScene().RemoveShape(this);
    Super::OnUnregister();
}

void UShapeComponent::OnTransformDirty()
{
    Super::OnTransformDirty();

    if (!IsRegistered() || !GetWorld())
        return;

    GetWorld()->GetCollisionScene().MarkBoundsDirty();
}

void UShapeComponent::QueryCandidates(UShapeComponent* Shape, TArray<UShapeComponent*>& OutCandidates)
{

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
