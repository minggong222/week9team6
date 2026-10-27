#include "EnginePCH.h"
#include "sol/sol.hpp"
#include "Components/LightComponentBase.h"
#include "Components/LightComponent.h"
#include "Components/LocalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/DirectionalLightComponent.h"

void LuaRegisterComponentLight(sol::state& State)
{
    // 현재 Lua 스크립트에서 사용하지 않는 열거형입니다.
    // State.new_enum("ELightType",
        // "Directional", ELightType::Directional,
        // "Point", ELightType::Point,
        // "Spot", ELightType::Spot,
        // "Rect", ELightType::Rect,
        // "Fireball", ELightType::Fireball,
        // "Sky", ELightType::Sky);

    State.new_usertype<ULightComponentBase>("ULightComponentBase", sol::no_constructor,
        sol::base_classes, sol::bases<USceneComponent, UActorComponent, UObject>(),
        // 현재 Lua 스크립트에서 참조하지 않는 멤버는 등록하지 않습니다.
        "StaticClass", &ULightComponentBase::StaticClass,
        "Cast", &Cast<ULightComponentBase>,
        // "GetIntensity", &ULightComponentBase::GetIntensity,
        // "GetLightColor", &ULightComponentBase::GetLightColor,
        // "CastsShadows", &ULightComponentBase::CastsShadows,
        "SetIntensity", &ULightComponentBase::SetIntensity,
        "SetLightColor", &ULightComponentBase::SetLightColor,
        "SetCastShadows", &ULightComponentBase::SetCastShadows
    );

    State.new_usertype<ULightComponent>("ULightComponent", sol::no_constructor,
        sol::base_classes, sol::bases<ULightComponentBase, USceneComponent, UActorComponent, UObject>(),
        // 현재 Lua 스크립트에서 참조하지 않는 멤버는 등록하지 않습니다.
        "StaticClass", &ULightComponent::StaticClass,
        "Cast", &Cast<ULightComponent>
        // "GetShadowResolutionScale", &ULightComponent::GetShadowResolutionScale,
        // "GetShadowBias", &ULightComponent::GetShadowBias,
        // "GetShadowSlopeBias", &ULightComponent::GetShadowSlopeBias,
        // "GetShadowSharpen", &ULightComponent::GetShadowSharpen,
        // "SetShadowResolutionScale", &ULightComponent::SetShadowResolutionScale,
        // "SetShadowBias", &ULightComponent::SetShadowBias,
        // "SetShadowSlopeBias", &ULightComponent::SetShadowSlopeBias,
        // "SetShadowSharpen", &ULightComponent::SetShadowSharpen,
        // "GetLightType", &ULightComponent::GetLightType
    );

    State.new_usertype<ULocalLightComponent>("ULocalLightComponent", sol::no_constructor,
        sol::base_classes, sol::bases<ULightComponent, ULightComponentBase, USceneComponent, UActorComponent, UObject>(),
        // 현재 Lua 스크립트에서 참조하지 않는 멤버는 등록하지 않습니다.
        "StaticClass", &ULocalLightComponent::StaticClass,
        "Cast", &Cast<ULocalLightComponent>,
        // "GetAttenuationRadius", &ULocalLightComponent::GetAttenuationRadius,
        "SetAttenuationRadius", &ULocalLightComponent::SetAttenuationRadius
    );

    State.new_usertype<UPointLightComponent>("UPointLightComponent", sol::no_constructor,
        sol::base_classes, sol::bases<ULocalLightComponent, ULightComponent, ULightComponentBase, USceneComponent, UActorComponent, UObject>(),
        // 현재 Lua 스크립트에서 참조하지 않는 멤버는 등록하지 않습니다.
        "StaticClass", &UPointLightComponent::StaticClass,
        "Cast", &Cast<UPointLightComponent>
        // "GetFalloff", &UPointLightComponent::GetFalloff,
        // "SetFalloff", &UPointLightComponent::SetFalloff,
        // "GetSourceRadius", &UPointLightComponent::GetSourceRadius,
        // "SetSourceRadius", &UPointLightComponent::SetSourceRadius
    );

    State.new_usertype<USpotLightComponent>("USpotLightComponent", sol::no_constructor,
        sol::base_classes, sol::bases<UPointLightComponent, ULocalLightComponent, ULightComponent, ULightComponentBase, USceneComponent, UActorComponent, UObject>(),
        // 현재 Lua 스크립트에서 참조하지 않는 멤버는 등록하지 않습니다.
        "StaticClass", &USpotLightComponent::StaticClass,
        "Cast", &Cast<USpotLightComponent>
        // "GetInnerConeAngle", &USpotLightComponent::GetInnerConeAngle,
        // "SetInnerConeAngle", &USpotLightComponent::SetInnerConeAngle,
        // "GetOuterConeAngle", &USpotLightComponent::GetOuterConeAngle,
        // "SetOuterConeAngle", &USpotLightComponent::SetOuterConeAngle
    );

    State.new_usertype<UDirectionalLightComponent>("UDirectionalLightComponent", sol::no_constructor,
        sol::base_classes, sol::bases<ULightComponent, ULightComponentBase, USceneComponent, UActorComponent, UObject>(),
        // 현재 Lua 스크립트에서 참조하지 않는 멤버는 등록하지 않습니다.
        "StaticClass", &UDirectionalLightComponent::StaticClass,
        "Cast", &Cast<UDirectionalLightComponent>
        // "GetAngularRadiusDegrees", &UDirectionalLightComponent::GetAngularRadiusDegrees,
        // "SetAngularRadiusDegrees", &UDirectionalLightComponent::SetAngularRadiusDegrees
    );
}
