#include "EnginePCH.h"
#include "sol/sol.hpp"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Camera/CameraComponent.h"

void LuaRegisterWorld(sol::state& State)
{
    State.new_usertype<UWorld>("UWorld", sol::no_constructor,
        sol::base_classes, sol::bases<UObject>(),
        "GetPlayerPawn", &UWorld::GetPlayerPawn,
        "GetMainCamera", &UWorld::GetMainCamera,
        "GetMainCameraComponent", &UWorld::GetMainCameraComponent,
        "GetPlayerCamera", &UWorld::GetPlayerCamera,
        "SetMainCameraComponent", &UWorld::SetMainCameraComponent,
        "SetCameraComponent", &UWorld::SetMainCameraComponent);
}
