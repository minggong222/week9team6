#pragma once

#include "GameFramework/Pawn.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraComponent.h"

class AStaticMeshActor :public APawn
{
	DECLARE_CLASS(AStaticMeshActor, APawn)
public:
	AStaticMeshActor();
	virtual ~AStaticMeshActor() = default;

	virtual void BeginPlay() override;

    void AddMovementInput(const FVector& WorldDirection, float ScaleValue) override;

    void AddControllerYawInput(float Value) override;
    void AddControllerPitchInput(float Value) override;

    UStaticMeshComponent* GetStaticMeshComponent() const { return StaticMeshComponent; }

    UCameraComponent* GetCameraComponent() const { return CameraComponent; }

    void SetPrimitiveType(EPrimitiveType Type);

private:
    UStaticMeshComponent* StaticMeshComponent = nullptr;
    UCameraComponent* CameraComponent = nullptr;

    float MoveSpeed = 20.0f;
};

