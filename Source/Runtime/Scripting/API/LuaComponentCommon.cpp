#include "EnginePCH.h"
#include "sol/sol.hpp"
#include "Camera/CameraComponent.h"
#include "Components/LuaComponent.h"
#include "Components/TextRenderComponent.h"

void LuaRegisterComponentCommon(sol::state& State)
{
    State.new_usertype<UCameraComponent>("UCameraComponent", sol::no_constructor,
        sol::base_classes, sol::bases<USceneComponent, UActorComponent, UObject>(),
        "StaticClass", &UCameraComponent::StaticClass,
        "Cast", &Cast<UCameraComponent>,
        "GetFieldOfView", &UCameraComponent::GetFieldOfView,
        "SetFieldOfView", &UCameraComponent::SetFieldOfView,
        "GetAspectRatio", &UCameraComponent::GetAspectRatio,
        "SetAspectRatio", &UCameraComponent::SetAspectRatio,
        "GetNearZ", &UCameraComponent::GetNearZ,
        "SetNearZ", &UCameraComponent::SetNearZ,
        "GetFarZ", &UCameraComponent::GetFarZ,
        "SetFarZ", &UCameraComponent::SetFarZ,
        "GetIsOrthogonal", &UCameraComponent::GetIsOrthogonal,
        "SetIsOrthogonal", &UCameraComponent::SetIsOrthogonal,
        "GetOrthoWidth", &UCameraComponent::GetOrthoWidth,
        "SetOrthoWidth", &UCameraComponent::SetOrthoWidth,
        "GetMoveSpeed", &UCameraComponent::GetMoveSpeed,
        "SetMoveSpeed", &UCameraComponent::SetMoveSpeed,
        "GetMouseSensitivity", &UCameraComponent::GetMouseSensitivity,
        "SetMouseSensitivity", &UCameraComponent::SetMouseSensitivity,
        "GetWheelSpeed", &UCameraComponent::GetWheelSpeed,
        "SetWheelSpeed", &UCameraComponent::SetWheelSpeed,
        "SetExternalInputManaged", &UCameraComponent::SetExternalInputManaged,
        "IsExternalInputManaged", &UCameraComponent::IsExternalInputManaged);

    State.new_usertype<ULuaComponent>("ULuaComponent", sol::no_constructor,
        sol::base_classes, sol::bases<UActorComponent, UObject>(),
        "StaticClass", &ULuaComponent::StaticClass,
        "Cast", &Cast<ULuaComponent>,
        "InitializeLua", &ULuaComponent::InitializeLua,
        "LoadLua", &ULuaComponent::LoadLua,
        "Invoke", &ULuaComponent::Invoke,
        "GetScriptPath", &ULuaComponent::GetScriptPath,
        "SetScriptPath", &ULuaComponent::SetScriptPath,
        "GetInitialized", &ULuaComponent::GetInitialized,
        "GetLoaded", &ULuaComponent::GetLoaded);

    State.new_usertype<UTextRenderComponent>("UTextRenderComponent", sol::no_constructor,
        sol::base_classes, sol::bases<UPrimitiveComponent, USceneComponent, UActorComponent, UObject>(),
        "StaticClass", &UTextRenderComponent::StaticClass,
        "Cast", &Cast<UTextRenderComponent>,
        "GetText", &UTextRenderComponent::GetText,
        "SetText", &UTextRenderComponent::SetText,
        "GetFont", &UTextRenderComponent::GetFont,
        "SetFont", &UTextRenderComponent::SetFont,
        "GetTextSize", &UTextRenderComponent::GetTextSize,
        "SetTextSize", &UTextRenderComponent::SetTextSize);
}
