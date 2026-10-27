#include "EnginePCH.h"
#include "sol/sol.hpp"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraActor.h"

void LuaRegisterWorld(sol::state& State)
{
    State.new_usertype<APlayerCameraManager>("APlayerCameraManager", sol::no_constructor,
        sol::base_classes, sol::bases<AActor, UObject>(),
        "StartCameraFade", &APlayerCameraManager::StartCameraFade,
        "StopCameraFade", &APlayerCameraManager::StopCameraFade,
        "GetFadeAmount", &APlayerCameraManager::GetFadeAmount,
        "SetLetterBox", &APlayerCameraManager::SetLetterBox,
        "StopLetterBox", &APlayerCameraManager::StopLetterBox);

    State.new_usertype<UWorld>("UWorld", sol::no_constructor,
        sol::base_classes, sol::bases<UObject>(),
        // 현재 Lua 스크립트에서 참조하지 않는 멤버는 등록하지 않습니다.
        "SpawnActor", [](UWorld& World, UClass* Class, sol::optional<FString> Name, sol::optional<FTransform> Transform) {
            if (!Class || !Class->IsChildOf(AActor::StaticClass()) || !Class->Constructor)
            {
                throw sol::error("SpawnActor requires a concrete actor class");
            }

            return World.SpawnActor(Class, Name ? FName(*Name) : NAME_None, Transform ? &*Transform : nullptr);
        },
        "GetPlayerPawn", &UWorld::GetPlayerPawn,
        "GetMainCamera", &UWorld::GetMainCamera,
        "GetMainCameraComponent", &UWorld::GetMainCameraComponent,
        "GetPlayerCamera", &UWorld::GetPlayerCamera,
        "GetPlayerCameraManager", [](UWorld& World) -> APlayerCameraManager* {
            APlayerController* Controller = World.GetPlayerController();
            return Controller ? Controller->GetPlayerCameraManager() : nullptr;
        },
        "GetGlobalTimeDilation", &UWorld::GetGlobalTimeDilation,
        "SetGlobalTimeDilation", &UWorld::SetGlobalTimeDilation,
        "SetMainCameraComponent",
        [](UWorld& World, UCameraComponent* Camera, sol::optional<float> BlendTime)
        {
            return World.SetMainCameraComponent(Camera, BlendTime ? *BlendTime : 0.0f);
        },

        "SetCameraComponent",
        [](UWorld& World, UCameraComponent* Camera, sol::optional<float> BlendTime)
        {
            return World.SetMainCameraComponent(Camera, BlendTime ? *BlendTime : 0.0f);
        },
        "StartCameraShake",
        [](UWorld& World, float Duration, float LocationAmplitude, float RotationAmplitude, float Frequency)
        {
            APlayerController* PlayerController = World.GetPlayerController();

            APlayerCameraManager* CameraManager = PlayerController ? PlayerController->GetPlayerCameraManager() : nullptr;

            return CameraManager && CameraManager->StartCameraShake(Duration, LocationAmplitude, RotationAmplitude, Frequency) != nullptr;
        }
    );
}
