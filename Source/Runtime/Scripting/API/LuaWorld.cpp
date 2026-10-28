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
	State.new_enum<ETimeMode>("ETimeMode", {
		{ "GameTime", ETimeMode::GameTime },
		{ "RealTime", ETimeMode::RealTime }
	});

    State.new_usertype<APlayerCameraManager>("APlayerCameraManager", sol::no_constructor,
        sol::base_classes, sol::bases<AActor, UObject>(),
        "StartCameraFade", &APlayerCameraManager::StartCameraFade,
        "StopCameraFade", &APlayerCameraManager::StopCameraFade,
        "GetFadeAmount", &APlayerCameraManager::GetFadeAmount,
        // "GetFadeColor", &APlayerCameraManager::GetFadeColor, // 현재 Lua에서는 미사용
        "SetLetterBox", &APlayerCameraManager::SetLetterBox,
        // "GetLetterBoxHeight", &APlayerCameraManager::GetLetterBoxHeight, // 현재 Lua에서는 미사용
        "StopLetterBox", &APlayerCameraManager::StopLetterBox,
        "StartCameraZoom", [](APlayerCameraManager& Manager, float Scale, float ZoomInDuration, float ZoomOutDuration, sol::optional<int32> Priority) {
            return Manager.StartCameraZoom(Scale, ZoomInDuration, ZoomOutDuration, Priority.value_or(0));
        },
        "StopCameraZoom", &APlayerCameraManager::StopCameraZoom);

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
        "GetDeltaSeconds", &UWorld::GetDeltaSeconds,
        "GetRealDeltaSeconds", &UWorld::GetRealDeltaSeconds,
        "PlaySlomo", [](UWorld& World, float Scale, float BlendIn, float Hold, float BlendOut, sol::optional<ETimeMode> Mode) {
            return World.PlaySlomo(Scale, BlendIn, Hold, BlendOut, Mode.value_or(ETimeMode::RealTime));
        },
        "BeginSlomo", [](UWorld& World, float Scale, float BlendIn, sol::optional<ETimeMode> Mode) {
            return World.BeginSlomo(Scale, BlendIn, Mode.value_or(ETimeMode::RealTime));
        },
        "EndSlomo", [](UWorld& World, float BlendOut, sol::optional<ETimeMode> Mode) {
            return World.EndSlomo(BlendOut, Mode.value_or(ETimeMode::RealTime));
        },
        "PlayHitStop", &UWorld::PlayHitStop,
        "ResetTimeEffects", &UWorld::ResetTimeEffects,
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
        [](UWorld& World, float Duration, float LocationAmplitude, float RotationAmplitude, float Frequency, sol::optional<int32> Priority)
        {
            APlayerController* PlayerController = World.GetPlayerController();

            APlayerCameraManager* CameraManager = PlayerController ? PlayerController->GetPlayerCameraManager() : nullptr;

            return CameraManager && CameraManager->StartCameraShake(Duration, LocationAmplitude, RotationAmplitude, Frequency, Priority ? *Priority : 0) != nullptr;
        }
    );
}
