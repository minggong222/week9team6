#include "EnginePCH.h"
#include "sol/sol.hpp"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Render/Material.h"
#include "Components/BoxComponent.h"

extern void LuaRegisterComponentBase(sol::state& State);
extern void LuaRegisterComponentCommon(sol::state& State);
extern void LuaRegisterComponentMesh(sol::state& State);
extern void LuaRegisterComponentMovement(sol::state& State);
extern void LuaRegisterComponentLight(sol::state& State);

void LuaRegisterComponent(sol::state& State)
{
    LuaRegisterComponentBase(State);
    LuaRegisterComponentCommon(State);
    LuaRegisterComponentMesh(State);
    LuaRegisterComponentMovement(State);
    LuaRegisterComponentLight(State);
}

void LuaRegisterComponentBase(sol::state& State)
{
    // 현재 Lua 스크립트에서 사용하지 않는 열거형입니다.
    // State.new_enum("EAttachmentRule",
        // "KeepRelative", EAttachmentRule::KeepRelative,
        // "KeepWorld", EAttachmentRule::KeepWorld,
        // "SnapToTarget", EAttachmentRule::SnapToTarget);

    State.new_enum("ECollisionResponse",
        "Ignore", ECollisionResponse::Ignore,
        "Overlap", ECollisionResponse::Overlap,
        "Block", ECollisionResponse::Block);
    State.new_enum("ECollisionEnabled",
        "NoCollision", ECollisionEnabled::NoCollision,
        "QueryOnly", ECollisionEnabled::QueryOnly);
    State.new_enum("ECollisionChannel",
        "WorldStatic", ECollisionChannel::WorldStatic,
        "WorldDynamic", ECollisionChannel::WorldDynamic,
        "Pawn", ECollisionChannel::Pawn,
        "Projectile", ECollisionChannel::Projectile);

    State.new_usertype<FBox>("FBox", sol::constructors<FBox()>(),
        "Min", &FBox::Min, "Max", &FBox::Max);
    State.new_usertype<FRay>("FRay", sol::constructors<FRay()>(),
        // 현재 Lua 스크립트에서 참조하지 않는 멤버는 등록하지 않습니다.
        "Origin", &FRay::Origin, "Direction", &FRay::Direction
        // "PointAt", &FRay::PointAt
    );
    State.new_usertype<FHitResult>("FHitResult", sol::no_constructor,
        // 현재 Lua 스크립트에서 참조하지 않는 멤버는 등록하지 않습니다.
        // "HitComponent", sol::readonly(&FHitResult::HitComponent),
        "Distance", sol::readonly(&FHitResult::Distance),
        // "Time", sol::readonly(&FHitResult::Time),
        "Location", sol::readonly(&FHitResult::Location)
        // "ImpactPoint", sol::readonly(&FHitResult::ImpactPoint),
        // "Normal", sol::readonly(&FHitResult::Normal),
        // "ImpactNormal", sol::readonly(&FHitResult::ImpactNormal),
        // "bBlockingHit", sol::readonly(&FHitResult::bBlockingHit)
    );

    State.new_usertype<UActorComponent>("UActorComponent", sol::no_constructor,
        sol::base_classes, sol::bases<UObject>(),
        // 현재 Lua 스크립트에서 참조하지 않는 멤버는 등록하지 않습니다.
        "StaticClass", &UActorComponent::StaticClass,
        "Cast", &Cast<UActorComponent>,
        // "GetOwner", &UActorComponent::GetOwner,
        "GetWorld", &UActorComponent::GetWorld,
        "Activate", sol::overload(&UActorComponent::Activate,
            [](UActorComponent& Component) { Component.Activate(); }),
        "Deactivate", &UActorComponent::Deactivate,
        // "SetActive", &UActorComponent::SetActive,
        "IsActive", &UActorComponent::IsActive,
        // "SetAutoActivate", &UActorComponent::SetAutoActivate,
        // "IsAutoActivate", &UActorComponent::IsAutoActivate,
        "SetVisibility", &UActorComponent::SetVisibility,
        // "IsVisible", &UActorComponent::IsVisible,
        // "SetHiddenInGame", &UActorComponent::SetHiddenInGame,
        // "IsHiddenInGame", &UActorComponent::IsHiddenInGame,
        "SetComponentTickEnabled", &UActorComponent::SetComponentTickEnabled
        // "IsComponentTickEnabled", &UActorComponent::IsComponentTickEnabled,
        // "HasBegunPlay", &UActorComponent::HasBegunPlay
    );

    State.new_usertype<USceneComponent>("USceneComponent", sol::no_constructor,
        sol::base_classes, sol::bases<UActorComponent, UObject>(),
        // 현재 Lua 스크립트에서 참조하지 않는 멤버는 등록하지 않습니다.
        "StaticClass", &USceneComponent::StaticClass,
        "Cast", &Cast<USceneComponent>,
        // Lua는 const &이 없기 때문에, 복사본을 반환. 
        // Transform을 수정할때는 반드시 Set을 사용
        "GetRelativeLocation", [](const USceneComponent& Component) { return Component.GetRelativeLocation(); },
        "GetRelativeRotation", [](const USceneComponent& Component) { return Component.GetRelativeRotation(); },
        "GetRelativeScale3D", [](const USceneComponent& Component) { return Component.GetRelativeScale3D(); },
        // "GetTransform", [](const USceneComponent& Component) { return Component.GetTransform(); },
        "SetRelativeLocation", &USceneComponent::SetRelativeLocation,
        "SetRelativeRotation", &USceneComponent::SetRelativeRotation,
        "SetRelativeScale3D", &USceneComponent::SetRelativeScale3D,
        // "SetRelativeLocationAndRotation", &USceneComponent::SetRelativeLocationAndRotation,
        // "SetTransform", &USceneComponent::SetTransform,
        // "GetRelativeRotationQuat", &USceneComponent::GetRelativeRotationQuat,
        "GetWorldLocation", &USceneComponent::GetWorldLocation,
        "GetWorldRotation", &USceneComponent::GetWorldRotation,
        "GetWorldScale3D", &USceneComponent::GetWorldScale3D,
        // "GetWorldMatrix", &USceneComponent::GetWorldMatrix,
        "SetWorldLocation", &USceneComponent::SetWorldLocation,
        "SetWorldRotation", &USceneComponent::SetWorldRotation,
        // "SetWorldLocationAndRotation", &USceneComponent::SetWorldLocationAndRotation,
        // "SetWorldTransform", &USceneComponent::SetWorldTransform,
        // "GetAttachParent", &USceneComponent::GetAttachParent,
        // "GetAttachChildren", [](const USceneComponent& Component) { return sol::as_table(Component.GetAttachChildren()); },
        "SetupAttachment", sol::overload(&USceneComponent::SetupAttachment,
            [](USceneComponent& Component, USceneComponent* Parent) { Component.SetupAttachment(Parent); }),
        // "DetachFromParent", sol::overload(&USceneComponent::DetachFromParent,
            // [](USceneComponent& Component) { Component.DetachFromParent(); }),
        "SetVisibility", sol::overload(
            sol::resolve<void(bool)>(&USceneComponent::SetVisibility),
            sol::resolve<void(bool, bool)>(&USceneComponent::SetVisibility)),
        // "SetHiddenInGame", sol::overload(
            // sol::resolve<void(bool)>(&USceneComponent::SetHiddenInGame),
            // sol::resolve<void(bool, bool)>(&USceneComponent::SetHiddenInGame)),
        "CalcBounds", &USceneComponent::CalcBounds,
        // "GetCollisionRadius", &USceneComponent::GetCollisionRadius,
        // "SetCollisionRadiusOverride", &USceneComponent::SetCollisionRadiusOverride,
        // "MoveComponent", [](USceneComponent& Component, const FVector& Delta, const FRotator& Rotation, bool Sweep) {
            // FHitResult Hit;
            // bool Success = Component.MoveComponent(Delta, Rotation, Sweep, &Hit);
            // return std::make_tuple(Success, Hit);
        // },
        "TryMoveWithBlocking", [](USceneComponent& Component, const FVector& Delta, const FRotator& Rotation)
        {
            UShapeComponent* BlockingShape = nullptr;
            const bool bMoved = Component.TryMoveWithBlocking(Delta, Rotation, &BlockingShape);
            return std::make_tuple(bMoved, static_cast<UPrimitiveComponent*>(BlockingShape));
        }
    );

    State.new_usertype<UPrimitiveComponent>("UPrimitiveComponent", sol::no_constructor,
        sol::base_classes, sol::bases<USceneComponent, UActorComponent, UObject>(),
        // 현재 Lua 스크립트에서 참조하지 않는 멤버는 등록하지 않습니다.
        "StaticClass", &UPrimitiveComponent::StaticClass,
        "Cast", &Cast<UPrimitiveComponent>,
        "OnComponentBeginOverlap", sol::readonly(&UPrimitiveComponent::OnComponentBeginOverlap),
        // "OnComponentEndOverlap", sol::readonly(&UPrimitiveComponent::OnComponentEndOverlap),
        // "OnComponentHit", sol::readonly(&UPrimitiveComponent::OnComponentHit),
        // "GetNumMaterials", &UPrimitiveComponent::GetNumMaterials,
        // "GetMaterial", &UPrimitiveComponent::GetMaterial,
        // "SetMaterial", &UPrimitiveComponent::SetMaterial,
        "SetCastShadow", &UPrimitiveComponent::SetCastShadow,
        // "SetReceiveShadow", &UPrimitiveComponent::SetReceiveShadow,
        // "LineTraceComponent", [](UPrimitiveComponent& Component, const FRay& Ray) {
            // FHitResult Hit;
            // bool Success = Component.LineTraceComponent(Ray, Hit);
            // return std::make_tuple(Success, Hit);
        // },
        "SetCollisionEnabled", &UPrimitiveComponent::SetCollisionEnabled,
        "SetCollisionObjectType", &UPrimitiveComponent::SetCollisionObjectType,
        "SetCollisionResponseToChannel", &UPrimitiveComponent::SetCollisionResponseToChannel,
        "SetCollisionResponseToAllChannels", &UPrimitiveComponent::SetCollisionResponseToAllChannels
    );
    State.new_usertype<UShapeComponent>("UShapeComponent", sol::no_constructor,
        sol::base_classes, sol::bases<UPrimitiveComponent, USceneComponent, UActorComponent, UObject>(),
        "Cast", &Cast<UShapeComponent>);
    State.new_usertype<UBoxComponent>("UBoxComponent", sol::no_constructor,
        sol::base_classes, sol::bases<UShapeComponent, UPrimitiveComponent, USceneComponent, UActorComponent, UObject>(),
        "Cast", &Cast<UBoxComponent>,
        "SetBoxExtent", &UBoxComponent::SetBoxExtent);
}
