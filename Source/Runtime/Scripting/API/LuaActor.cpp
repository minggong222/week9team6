#include "EnginePCH.h"
#include "sol/sol.hpp"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Engine/Level.h"

void LuaRegisterActor(sol::state& State)
{
    State.new_usertype<AActor>("AActor", sol::no_constructor,
        sol::base_classes, sol::bases<UObject>(),
        "StaticClass", &AActor::StaticClass,
        "Cast", &Cast<AActor>,
        "GetWorld", &AActor::GetWorld,
        "GetLevel", &AActor::GetLevel,
        "GetActorLocation", &AActor::GetActorLocation,
        "GetActorRotation", &AActor::GetActorRotation,
        "GetActorScale3D", &AActor::GetActorScale3D,
        "GetActorQuat", &AActor::GetActorQuat,
        "GetActorTransform", &AActor::GetActorTransform,
        "GetRootComponent", &AActor::GetRootComponent,
        "GetComponents", [](const AActor& Actor) { return sol::as_table(Actor.GetComponents()); },
        "FindComponentByName", [](const AActor& Actor, const FString& Name) {
            return Actor.FindComponentByName(FName(Name));
        },
        "AddComponent", [](AActor& Actor, UClass* Class, const FString& Name) {
            if (!Class || !Class->IsChildOf(UActorComponent::StaticClass()) || !Class->Constructor)
                throw sol::error("AddComponent requires a concrete component class");
            return Actor.AddComponent(Class, FName(Name));
        },
        "DestroyComponent", [](AActor& Actor, UActorComponent* Component) {
            if (Component && Component->GetOwner() != &Actor)
                throw sol::error("Component does not belong to this actor");
            if (Component) Actor.DestroyComponent(Component);
        },
        "TryGetActorBounds", [](const AActor& Actor) {
            FBox Bounds{};
            bool Success = Actor.TryGetActorBounds(Bounds);
            return std::make_tuple(Success, Bounds);
        },
        "SetActorEnableCollision", &AActor::SetActorEnableCollision,
        "GetActorEnableCollision", &AActor::GetActorEnableCollision,
        "HasBegunPlay", &AActor::HasBegunPlay,
        "Destroy", &AActor::Destroy
    );
}
