#include "EnginePCH.h"
#include "sol/sol.hpp"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Camera/CameraComponent.h"

void LuaRegisterWorld(sol::state& State)
{
    State.new_usertype<UWorld>("UWorld", sol::no_constructor,
        sol::base_classes, sol::bases<UObject>(),
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
        "SetMainCameraComponent",
        [](UWorld& World, UCameraComponent* Camera, sol::optional<float> BlendTime)
        {
            return World.SetMainCameraComponent(
                Camera,
                BlendTime ? *BlendTime : 0.0f);
        },

        "SetCameraComponent",
        [](UWorld& World, UCameraComponent* Camera, sol::optional<float> BlendTime)
        {
            return World.SetMainCameraComponent(
                Camera,
                BlendTime ? *BlendTime : 0.0f);
        });
}
