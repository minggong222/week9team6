#include "EnginePCH.h"
#include "sol/sol.hpp"
#include "Asset/AssetManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/MeshComponent.h"

void LuaRegisterComponentMesh(sol::state& State)
{
    // Lua에는 템플릿 문법이 없으므로 UStaticMesh 조회로 바인딩합니다.
    State.create_named_table("UAssetManager",
        "GetAssetByPath", sol::overload(
            &UAssetManager::GetAssetByPath<UStaticMesh>,
            [](sol::table, const FString& Path)
            {
                return UAssetManager::GetAssetByPath<UStaticMesh>(Path);
            }));

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
