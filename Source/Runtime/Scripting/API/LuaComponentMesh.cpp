#include "EnginePCH.h"
#include "sol/sol.hpp"
#include "Asset/AssetManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/MeshComponent.h"
#include "Render/Material.h"
#include "Math/Color.h"

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
        // 현재 Lua 스크립트에서 참조하지 않는 멤버는 등록하지 않습니다.
        "StaticClass", &UMeshComponent::StaticClass,
        "Cast", &Cast<UMeshComponent>
        // "GetMaterialSlotName", &UMeshComponent::GetMaterialSlotName,
        // "GetDefaultMaterial", &UMeshComponent::GetDefaultMaterial,
        // "GetOverrideMaterial", &UMeshComponent::GetOverrideMaterial,
        // "ClearOverrideMaterials", &UMeshComponent::ClearOverrideMaterials
    );

    State.new_usertype<UStaticMeshComponent>("UStaticMeshComponent", sol::no_constructor,
        sol::base_classes, sol::bases<UMeshComponent, UPrimitiveComponent, USceneComponent, UActorComponent, UObject>(),
        "StaticClass", &UStaticMeshComponent::StaticClass,
        "Cast", &Cast<UStaticMeshComponent>,
        "SetStaticMesh", &UStaticMeshComponent::SetStaticMesh,
        "GetStaticMesh", &UStaticMeshComponent::GetStaticMesh,
        "SetUnlitColor", [](UStaticMeshComponent& Mesh, const FColor& Color) {
            UMaterial* Material = Mesh.GetOverrideMaterial(0);
            if (!Material || !Material->bIsInstance)
                Material = UMaterial::CreateInstance(Mesh.GetMaterial(0));
            if (!Material) throw sol::error("SetUnlitColor requires a mesh material");

            const FLinearColor Linear(Color);
            Material->BaseColor = FVector4(Linear.R, Linear.G, Linear.B, Linear.A);
            Material->bUnlit = true;
            Mesh.SetMaterial(0, Material);
        });
}
