#include "EnginePCH.h"
#include "sol/sol.hpp"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "GameFramework/Pawn.h"
#include "Camera/CameraActor.h"
#include "Camera/CineCameraActor.h"
void LuaRegisterActor(sol::state& State)
{
    State.new_usertype<AActor>("AActor", sol::no_constructor,
        sol::base_classes, sol::bases<UObject>(),
        // 현재 Lua 스크립트에서 참조하지 않는 멤버는 등록하지 않습니다.
        "StaticClass", &AActor::StaticClass,
        "Cast", &Cast<AActor>,
        "GetWorld", &AActor::GetWorld,
        // "GetLevel", &AActor::GetLevel,
        "GetActorLocation", &AActor::GetActorLocation,
        // "GetActorRotation", &AActor::GetActorRotation,
        // "GetActorScale3D", &AActor::GetActorScale3D,
        // "GetActorQuat", &AActor::GetActorQuat,
        // "GetActorTransform", &AActor::GetActorTransform,
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
        // "DestroyComponent", [](AActor& Actor, UActorComponent* Component) {
            // if (Component && Component->GetOwner() != &Actor)
                // throw sol::error("Component does not belong to this actor");
            // if (Component) Actor.DestroyComponent(Component);
        // },
        "TryGetActorBounds", [](const AActor& Actor) {
            FBox Bounds{};
            bool Success = Actor.TryGetActorBounds(Bounds);
            return std::make_tuple(Success, Bounds);
        },
        "SetActorEnableCollision", &AActor::SetActorEnableCollision,
        // "GetActorEnableCollision", &AActor::GetActorEnableCollision,
        // "HasBegunPlay", &AActor::HasBegunPlay,
        "Destroy", &AActor::Destroy
    );

    State.new_usertype<APawn>("APawn", sol::no_constructor,
        sol::base_classes, sol::bases<AActor, UObject>(),
        // 현재 Lua 스크립트에서 참조하지 않는 멤버는 등록하지 않습니다.
        "StaticClass", &APawn::StaticClass,
        "Cast", &Cast<APawn>
        // "AddMovementInput", &APawn::AddMovementInput,
        // "AddControllerYawInput", &APawn::AddControllerYawInput,
        // "AddControllerPitchInput", &APawn::AddControllerPitchInput
    );

    State.new_usertype<ACameraActor>("ACameraActor", sol::no_constructor,
        sol::base_classes, sol::bases<AActor, UObject>(),
        // 현재 Lua 스크립트에서 참조하지 않는 멤버는 등록하지 않습니다.
        "StaticClass", &ACameraActor::StaticClass,
        "Cast", &Cast<ACameraActor>,
        "GetCameraComponent", &ACameraActor::GetCameraComponent);

    State.new_usertype<ACineCameraActor>("ACineCameraActor", sol::no_constructor,
        sol::base_classes, sol::bases<ACameraActor, AActor, UObject>(),
        "StaticClass", &ACineCameraActor::StaticClass,
        "Cast", &Cast<ACineCameraActor>,
        "GetCameraComponent", &ACameraActor::GetCameraComponent,
        "GetSensorWidth", &ACineCameraActor::GetSensorWidth,
        "SetSensorWidth", &ACineCameraActor::SetSensorWidth,
        "GetFocalLength",  &ACineCameraActor::GetFocalLength,
        "SetFocalLength", &ACineCameraActor::SetFocalLength
    );
}
