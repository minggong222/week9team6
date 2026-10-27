#include "EnginePCH.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/ViewportCameraMovement.h"
#include <cmath>
#include "Math/Frustum.h"
#include "Render/RenderCommand.h"
#include "Camera/PlayerCameraManager.h"

FGameViewportClient::FGameViewportClient()
{
}

void FGameViewportClient::Init(FWorldContext& InWorldContext, UEngine* InEngine)
{
	SetWorld(InWorldContext.World());
	GameInstance = InWorldContext.OwningGameInstance;
	Engine = InEngine;
}

UWorld* FGameViewportClient::GetWorld() const
{
	return World;
}

void FGameViewportClient::MouseMove(FViewport* Viewport, int32 X, int32 Y)
{
	HandleUIMouseMove(X, Y);
}

bool FGameViewportClient::InputKey(FViewport* Viewport, int32 Key, bool bDown)
{
	if (HandleUIKey(Key, bDown))
		return true;
	if (bDown && Key == static_cast<int32>(EKeyCode::Escape))
	{
		bExitRequested = true;
		return true;
	}

	if (bDown && Key == static_cast<int32>(EKeyCode::F8))
	{
		bSIEModeRequested = true;
		return true;
	}

	if (bSIEMode && Key == static_cast<int32>(EKeyCode::LButton)) return true;

	return World && !World->IsPaused();
}

bool FGameViewportClient::InputAxis(FViewport* Viewport, int32 ControllerId, EGameInputAxis AxisKey, float Delta, float DeltaTime)
{
	if (HandleUIAxis(AxisKey, Delta))
		return true;

	return World && !World->IsPaused();
}

void FGameViewportClient::LostFocus()
{
	std::fill(std::begin(bSIEKeyDown), std::end(bSIEKeyDown), false);
	SIEMouseDeltaX = SIEMouseDeltaY = SIEWheelDelta = 0.0f;
	if (World)
	{
		if (APlayerController* PC = World->GetPlayerController())
			PC->ClearInputState();
	}
}

UCameraComponent* FGameViewportClient::FindPlayerCamera() const
{
	return World ? World->GetPlayerCamera() : nullptr;
}

void FGameViewportClient::SetWorld(UWorld* InWorld)
{
	if (World == InWorld) return;
	EndSIEMode();
	LostFocus();
	RenderData.ResetScene();
	CameraComponent = nullptr;
	bExitRequested = bSIEModeRequested = false;
	World = InWorld;
}

namespace
{
	bool SetCameraWorldPose(UCameraComponent& Camera, const FVector& Location, const FRotator& Rotation)
	{
		FTransform Relative = Camera.GetTransform();
		if (const USceneComponent* Parent = Camera.GetAttachParent())
		{
			const FMatrix ParentWorld = Parent->GetWorldMatrix();
			const float Determinant = ParentWorld.Determinant();
			if (!std::isfinite(Determinant) || Determinant == 0.0f)
				return false;
			Relative.Location = ParentWorld.Inverse().TransformPosition(Location);
			if (!std::isfinite(Relative.Location.X) || !std::isfinite(Relative.Location.Y) ||
				!std::isfinite(Relative.Location.Z)) return false;
			Relative.Rotation = (Parent->GetWorldRotation().Quaternion().Inverse() *
				Rotation.Quaternion()).ToFRotator();
		}
		else
		{
			Relative.Location = Location;
			Relative.Rotation = Rotation;
		}
		Camera.SetTransform(Relative);
		return true;
	}
}

FSceneView FGameViewportClient::CalcSceneView(const FRect& InViewRect)
{
	FSceneView OutView;
	OutView.ViewIndex = 0;
	OutView.bGameView = !bSIEMode;
	OutView.ViewRect = InViewRect;

	Resize(
		static_cast<uint32>(std::max(InViewRect.Width, 1.0f)),
		static_cast<uint32>(std::max(InViewRect.Height, 1.0f))
	);

	const float Aspect = static_cast<float>(Width) / static_cast<float>(Height);

	FCameraPOV POV;
	bool bHasPOV = false;

	APlayerCameraManager* CameraManager = nullptr;

	if (!bSIEMode && World)
	{
		APlayerController* PlayerController = World->GetPlayerController();

		if (PlayerController)
		{
			CameraManager = PlayerController->GetPlayerCameraManager();
		}

		if (CameraManager && CameraManager->GetCameraPOV(POV))
		{
			CameraComponent = CameraManager->GetViewTargetCamera();
			POV.AspectRatio = Aspect;
			bHasPOV = POV.IsValid();
		}
	}

	if (bHasPOV)
	{
		if (CameraManager && !bSIEMode && World)
		{
			CameraComponent = FindPlayerCamera();

			if (!CameraComponent)
			{
				CameraComponent = World->GetMainCameraComponent();
			}
		}
		else if (!CameraComponent && World)
		{
			CameraComponent = FindPlayerCamera();
			if (!CameraComponent)
			{
				CameraComponent = World->GetMainCameraComponent();
			}
		}

		if (CameraComponent)
		{
			POV = CameraComponent->GetCameraPOV();
			POV.AspectRatio = Aspect;
			bHasPOV = POV.IsValid();
		}
	}

	// 어떤 카메라도 사용할 수 없을 때의 기본 시점.
	if (!bHasPOV)
	{
		POV = FCameraPOV{};
		POV.Location = FVector(-5.0f, -10.0f, 10.0f);
		POV.Rotation = FRotator(30.0f, 45.0f, 0.0f);
		POV.FieldOfView = 60.0f;
		POV.AspectRatio = Aspect;
		POV.NearClip = 0.1f;
		POV.FarClip = 1000.0f;
	}

	OutView.ViewLocation = POV.Location;
	OutView.ViewRotation = POV.Rotation;
	OutView.ViewForward = POV.Rotation.Quaternion().GetForwardVector();

	OutView.ViewMatrix = POV.GetViewMatrix();
	OutView.ProjectionMatrix = POV.GetProjectionMatrix();
	OutView.ViewProjectionMatrix = POV.GetViewProjectionMatrix();

	OutView.FieldOfView = POV.FieldOfView;
	OutView.AspectRatio = POV.AspectRatio;
	OutView.NearClip = POV.NearClip;
	OutView.FarClip = POV.FarClip;
	OutView.OrthoWidth = POV.OrthoWidth;
	OutView.bIsPerspective = POV.bIsPerspective;
	OutView.bIsWireframe = false;

	OutView.Frustum = ExtractFrustumPlanes(OutView.ViewProjectionMatrix);
	OutView.FrustumCorners = GenerateFrustumCorners(OutView.ViewProjectionMatrix.Inverse());

	return OutView;
}

void FGameViewportClient::SetCameraComponent(UCameraComponent* InCameraComponent)
{
	CameraComponent = InCameraComponent;
}

bool FGameViewportClient::BeginSIEMode()
{
	if (bSIEMode)
		return true;
	if (!World || !World->GetMainCamera())
		return false;

	if (!CameraComponent) CameraComponent = FindPlayerCamera();
	if (!CameraComponent) return false;

	UCameraComponent* MainCamera = World->GetMainCamera()->GetCameraComponent();
	if (!MainCamera || MainCamera == CameraComponent) return false;
	if (!SetCameraWorldPose(*MainCamera, CameraComponent->GetWorldLocation(),
		CameraComponent->GetWorldRotation())) return false;

	MainCamera->SetFieldOfView(CameraComponent->GetFieldOfView());
	MainCamera->SetNearZ(CameraComponent->GetNearZ());
	MainCamera->SetFarZ(CameraComponent->GetFarZ());
	MainCamera->SetIsOrthogonal(CameraComponent->GetIsOrthogonal());
	MainCamera->SetOrthoWidth(CameraComponent->GetOrthoWidth());
	bPreviousMainCameraExternalInputManaged = MainCamera->IsExternalInputManaged();
	MainCamera->SetExternalInputManaged(true);
	CameraComponent = MainCamera;
	bSIEMode = true;
	LostFocus();
	return true;
}

void FGameViewportClient::EndSIEMode()
{
	if (!bSIEMode)
		return;
	if (World && World->GetMainCamera())
		if (UCameraComponent* MainCamera = World->GetMainCamera()->GetCameraComponent())
			MainCamera->SetExternalInputManaged(bPreviousMainCameraExternalInputManaged);
	CameraComponent = FindPlayerCamera();
	bSIEMode = false;
	LostFocus();
}

void FGameViewportClient::GetKeyInputBySIEMode(int32 Key, bool bDown)
{
	if (bSIEMode && Key >= 0 && Key < 256)
		bSIEKeyDown[Key] = bDown;
}

void FGameViewportClient::GetAxisInputBySIEMode(EGameInputAxis Key, float Delta)
{
	if (!bSIEMode)
		return;
	switch (Key)
	{
	case EGameInputAxis::MouseX: SIEMouseDeltaX += Delta; break;
	case EGameInputAxis::MouseY: SIEMouseDeltaY += Delta; break;
	case EGameInputAxis::MouseWheel: SIEWheelDelta += Delta; break;
	}
}

void FGameViewportClient::TickSIEInput(float DeltaTime, float MoveSpeed, float MouseSensitivity)
{
	if (!bSIEMode || !CameraComponent)
		return;
	FViewportCameraInput Input;
	Input.bCaptured = bSIEKeyDown[static_cast<int32>(EKeyCode::RButton)];
	Input.bAllowWheel = true;
	Input.MouseX = SIEMouseDeltaX;
	Input.MouseY = SIEMouseDeltaY;
	Input.Wheel = SIEWheelDelta;
	Input.bForward = bSIEKeyDown[static_cast<int32>(EKeyCode::W)];
	Input.bBackward = bSIEKeyDown[static_cast<int32>(EKeyCode::S)];
	Input.bRight = bSIEKeyDown[static_cast<int32>(EKeyCode::D)];
	Input.bLeft = bSIEKeyDown[static_cast<int32>(EKeyCode::A)];
	Input.bUp = bSIEKeyDown[static_cast<int32>(EKeyCode::E)];
	Input.bDown = bSIEKeyDown[static_cast<int32>(EKeyCode::Q)];
	FViewportCameraState Camera{CameraComponent->GetWorldLocation(),
		CameraComponent->GetWorldRotation(), CameraComponent->GetOrthoWidth(),
		Width > 0 ? static_cast<float>(Width) : 800.0f, !CameraComponent->GetIsOrthogonal()};
	ApplyViewportCameraMovement(Camera, Input, DeltaTime, MoveSpeed, MouseSensitivity);
	if (SetCameraWorldPose(*CameraComponent, Camera.Location, Camera.Rotation))
		CameraComponent->SetOrthoWidth(Camera.OrthoWidth);
	SIEMouseDeltaX = SIEMouseDeltaY = SIEWheelDelta = 0.0f;
}

void FGameViewportClient::Resize(uint32 InWidth, uint32 InHeight)
{
	if (Width == InWidth && Height == InHeight && ViewTargets.IsValidFor(InWidth, InHeight))
	{
		return;
	}

	Width = InWidth;
	Height = InHeight;

	if (Width == 0 || Height == 0)
	{
		ViewTargets.Reset();
		return;
	}

	ViewTargets.Resize(Width, Height);
}

void FGameViewportClient::Reset()
{
	RenderData.ResetScene();
	EndSIEMode();
	LostFocus();
	bExitRequested = false;
	bSIEModeRequested = false;
	World = nullptr;
	GameInstance = nullptr;
	Engine = nullptr;
	CameraComponent = nullptr;

	ViewTargets.Reset();
	Width = 0;
	Height = 0;
}
