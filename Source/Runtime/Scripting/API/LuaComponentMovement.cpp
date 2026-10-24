#include "EnginePCH.h"
#include "sol/sol.hpp"
#include "Components/MovementComponent.h"
#include "Components/ProjectileMovementComponent.h"
#include "Components/RotatingMovementComponent.h"

void LuaRegisterComponentMovement(sol::state& State)
{
    State.new_usertype<UMovementComponent>("UMovementComponent", sol::no_constructor,
        sol::base_classes, sol::bases<UActorComponent, UObject>(),
        "StaticClass", &UMovementComponent::StaticClass,
        "Cast", &Cast<UMovementComponent>,
        "Velocity", &UMovementComponent::Velocity,
        "Gravity", &UMovementComponent::Gravity,
        "SetUpdatedComponent", &UMovementComponent::SetUpdatedComponent,
        "UpdateComponentVelocity", &UMovementComponent::UpdateComponentVelocity,
        "ComputeSlideVector", &UMovementComponent::ComputeSlideVector,
        "StopMovementImmediately", &UMovementComponent::StopMovementImmediately,
        "HandleImpact", sol::overload(&UMovementComponent::HandleImpact,
            [](UMovementComponent& Component, const FHitResult& Hit) { Component.HandleImpact(Hit); },
            [](UMovementComponent& Component, const FHitResult& Hit, float TimeSlice) { Component.HandleImpact(Hit, TimeSlice); }),
        "MoveUpdatedComponent", [](UMovementComponent& Component, const FVector& Delta, const FRotator& Rotation, bool Sweep) {
            FHitResult Hit;
            bool Success = Component.MoveUpdatedComponent(Delta, Rotation, Sweep, &Hit);
            return std::make_tuple(Success, Hit);
        });

    State.new_usertype<UProjectileMovementComponent>("UProjectileMovementComponent", sol::no_constructor,
        sol::base_classes, sol::bases<UMovementComponent, UActorComponent, UObject>(),
        "StaticClass", &UProjectileMovementComponent::StaticClass,
        "Cast", &Cast<UProjectileMovementComponent>,
        "InintialSpeed", &UProjectileMovementComponent::InintialSpeed,
        "MaxSpeed", &UProjectileMovementComponent::MaxSpeed,
        "ProjectileGravityScale", &UProjectileMovementComponent::ProjectileGravityScale,
        "bRotationFollowsVelocity", &UProjectileMovementComponent::bRotationFollowsVelocity,
        "bInitialVelocityInLocalSpace", &UProjectileMovementComponent::bInitialVelocityInLocalSpace,
        "bShouldBounce", &UProjectileMovementComponent::bShouldBounce,
        "Bounciness", &UProjectileMovementComponent::Bounciness,
        "Friction", &UProjectileMovementComponent::Friction,
        "bSimulationEnabled", &UProjectileMovementComponent::bSimulationEnabled,
        "bSweepCollision", &UProjectileMovementComponent::bSweepCollision,
        "MaxSimulationTimeStep", &UProjectileMovementComponent::MaxSimulationTimeStep,
        "MaxSimulationIterations", &UProjectileMovementComponent::MaxSimulationIterations,
        "bIsHomingProjectile", &UProjectileMovementComponent::bIsHomingProjectile,
        "HomingAccelerationMagnitude", &UProjectileMovementComponent::HomingAccelerationMagnitude,
        "HomingTargetComponent", &UProjectileMovementComponent::HomingTargetComponent,
        "SetVelocityInLocalSpace", &UProjectileMovementComponent::SetVelocityInLocalSpace,
        "LimitVelocity", &UProjectileMovementComponent::LimitVelocity,
        "ComputeVelocity", &UProjectileMovementComponent::ComputeVelocity,
        "ComputeMoveDelta", &UProjectileMovementComponent::ComputeMoveDelta,
        "ComputAcceleration", &UProjectileMovementComponent::ComputAcceleration,
        "ComputeHomingAcceleration", &UProjectileMovementComponent::ComputeHomingAcceleration,
        "ComputeBounceResult", &UProjectileMovementComponent::ComputeBounceResult,
        "AddForce", &UProjectileMovementComponent::AddForce,
        "GetPendingForce", &UProjectileMovementComponent::GetPendingForce,
        "GetSimulationTimeStep", &UProjectileMovementComponent::GetSimulationTimeStep,
        "HasStoppedSimulation", &UProjectileMovementComponent::HasStoppedSimulation,
        "ClearPendingForce", sol::overload(&UProjectileMovementComponent::ClearPendingForce,
            [](UProjectileMovementComponent& Component) { Component.ClearPendingForce(); }));

    State.new_usertype<URotatingMovementComponent>("URotatingMovementComponent", sol::no_constructor,
        sol::base_classes, sol::bases<UMovementComponent, UActorComponent, UObject>(),
        "StaticClass", &URotatingMovementComponent::StaticClass,
        "Cast", &Cast<URotatingMovementComponent>,
        "RotationRate", &URotatingMovementComponent::RotationRate,
        "PivotTranslation", &URotatingMovementComponent::PivotTranslation,
        "bRotaionInLocalSpace", &URotatingMovementComponent::bRotaionInLocalSpace);
}
