#include "EnginePCH.h"
#include "ShapeComponent.h"

#include "BoxComponent.h"
#include "CapsuleComponent.h"
#include "SphereComponent.h"
#include "Engine/World.h"

UShapeComponent::UShapeComponent()
{
	bHiddenInGame = true;
	bCastShadow = false;
	bReceiveShadow = false;
}

void UShapeComponent::PostEditChangeProperty(const FProperty& Property)
{
	Super::PostEditChangeProperty(Property);

	UBoxComponent* Box = Cast<UBoxComponent>(this);
	USphereComponent* Sphere = Cast<USphereComponent>(this);
	UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(this);

	if (Box && Property.Name == "BoxExtent")
	{
		Box->SetBoxExtent(Box->GetUnscaledBoxExtent());
	}
	else if (Sphere && Property.Name == "SphereRadius")
	{
		Sphere->SetSphereRadius(Sphere->GetUnscaledSphereRadius());
	}
	else if (Capsule && (Property.Name == "CapsuleRadius" || Property.Name == "CapsuleHalfHeight"))
	{
		Capsule->SetCapsuleSize(Capsule->GetUnscaledCapsuleRadius(), Capsule->GetUnscaledCapsuleHalfHeight());
	}
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

void UShapeComponent::MarkCollisionBoundsDirty()
{
	if (UWorld* World = GetWorld(); IsRegistered() && World)
		World->GetCollisionScene().MarkBoundsDirty();
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
