#include "EnginePCH.h"
#include "sol/sol.hpp"
#include "Components/StaticMeshComponent.h"
#include "Components/MeshComponent.h"

void LuaRegisterComponentMesh(sol::state& State)
{
    State.new_usertype<UMeshComponent>("UMeshComponent", sol::no_constructor,
        sol::base_classes, sol::bases<UPrimitiveComponent, USceneComponent, UActorComponent, UObject>(),
        "StaticClass", &UMeshComponent::StaticClass,
        "Cast", &Cast<UMeshComponent>,
        "GetMaterialSlotName", &UMeshComponent::GetMaterialSlotName,
        "GetDefaultMaterial", &UMeshComponent::GetDefaultMaterial,
        "GetOverrideMaterial", &UMeshComponent::GetOverrideMaterial,
        "ClearOverrideMaterials", &UMeshComponent::ClearOverrideMaterials);

    State.new_usertype<UStaticMeshComponent>("UStaticMeshComponent", sol::no_constructor,
        sol::base_classes, sol::bases<UMeshComponent, UPrimitiveComponent, USceneComponent, UActorComponent, UObject>(),
        "StaticClass", &UStaticMeshComponent::StaticClass,
        "Cast", &Cast<UStaticMeshComponent>,
        "SetStaticMesh", &UStaticMeshComponent::SetStaticMesh,
        "GetStaticMesh", &UStaticMeshComponent::GetStaticMesh);
}
