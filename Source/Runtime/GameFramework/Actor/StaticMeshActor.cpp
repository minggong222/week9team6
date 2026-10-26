#include "EnginePCH.h"
#include "StaticMeshActor.h"

#include "Camera/CameraComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Asset/AssetManager.h"

AStaticMeshActor::AStaticMeshActor()
{
	StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("UStaticMeshComponent");
	StaticMeshComponent->SetupAttachment(GetRootComponent());
    CameraComponent = CreateDefaultSubobject<UCameraComponent>("CameraComponent");

    CameraComponent->SetupAttachment(GetRootComponent());
    CameraComponent->SetRelativeLocation(FVector(-5.0f, 0.0f, 8.0f));

    CameraComponent->SetExternalInputManaged(true);
    CameraComponent->SetUseForPlayerView(true);
}

void AStaticMeshActor::SetPrimitiveType(EPrimitiveType Type)
{

}


void AStaticMeshActor::BeginPlay()
{
	Super::BeginPlay();
}

void AStaticMeshActor::AddMovementInput(const FVector& WorldDirection, float ScaleValue)
{
    if (GetRootComponent())
    {
        GetRootComponent()->SetRelativeLocation(GetActorLocation() + WorldDirection * (ScaleValue * MoveSpeed));
    }
}

void AStaticMeshActor::AddControllerYawInput(float Value)
{
    if (!GetRootComponent())
        return;

    FRotator Rotation = GetActorRotation();
    Rotation.Yaw += Value;

    GetRootComponent()->SetRelativeRotation(Rotation);
}

void AStaticMeshActor::AddControllerPitchInput(float Value)
{
    if (!GetRootComponent())
        return;

    FRotator Rotation = GetActorRotation();
    Rotation.Pitch = FMath::Clamp(Rotation.Pitch + Value, -89.0f, 89.0f);

    GetRootComponent()->SetRelativeRotation(Rotation);
}
