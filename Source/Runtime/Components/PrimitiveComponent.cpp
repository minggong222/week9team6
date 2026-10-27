#include "EnginePCH.h"
#include "PrimitiveComponent.h"
#include "Render/StaticMeshData.h"
#include "../Render/Renderer.h"
#include "Asset/AssetManager.h"
#include "Render/RenderCommand.h"

#include "Engine/PrimitiveSceneProxy.h"
#include "Engine/Scene.h"
#include "Engine/World.h"

namespace
{
	FString PrimitiveTypeToString(EPrimitiveType Type)
	{
		switch (Type)
		{
		case EPrimitiveType::Sphere:
			return "Sphere";
			break;
		case EPrimitiveType::Cube:
			return "Cube";
			break;
		case EPrimitiveType::Cone:
			return "Cone";
			break;
		case EPrimitiveType::Plane:
			return "Plane";
			break;
		default:
			return "";
			break;
		}
	}
}

UPrimitiveComponent::UPrimitiveComponent()
{
	//SetMaterial(UAssetManager::GetAssetByPath<UMaterial>("DefaultMaterial"));
	CollisionResponses.fill(ECollisionResponse::Block);
}

UPrimitiveComponent::~UPrimitiveComponent()
{

}

void UPrimitiveComponent::BeginPlay()
{
	Super::BeginPlay();
}

FBox UPrimitiveComponent::CalcLocalBounds() const
{
	const FStaticMeshData* Data = GetMeshData();
	return Data ? Data->AABB : Super::CalcLocalBounds();
}

void UPrimitiveComponent::SubmitToRenderQueue(FRenderQueue& RenderQueue, const FViewContext& ViewContext)
{
}

bool UPrimitiveComponent::LineTraceComponent(const FRay& WorldRay, FHitResult& OutHit)
{
	const FStaticMeshData* Mesh = GetMeshData();
	return Mesh && TraceMesh(WorldRay, *Mesh, GetWorldMatrix(), OutHit);
}

void UPrimitiveComponent::MarkRenderStateDirty()
{
	if (SceneProxy)
		SceneProxy->GetScene()->MarkRenderStateDirty(SceneProxy);
}

bool UPrimitiveComponent::LineTraceComponentLocal(const FRay& LocalRay, float& OutT)
{
	const FStaticMeshData* Mesh = GetMeshData();
	return Mesh && TraceMeshLocal(LocalRay, *Mesh, OutT);
}

void UPrimitiveComponent::OnTransformDirty()
{
	if (SceneProxy)
		SceneProxy->GetScene()->MarkDirty(SceneProxy);
}

bool UPrimitiveComponent::MoveComponent(const FVector& Delta, const FRotator& NewRotation, bool bSweep, FHitResult* Hit)
{
	return Super::MoveComponent(Delta, NewRotation, bSweep, Hit);
}

void UPrimitiveComponent::PostEditChangeProperty(const FProperty& Property)
{
	Super::PostEditChangeProperty(Property);

	if (Property.Name == "bCastShadow" ||
		Property.Name == "bReceiveShadow")
	{
		MarkRenderStateDirty();
	}
}

void UPrimitiveComponent::Serialize(FStructuredArchive::FRecord Record)
{
	Super::Serialize(Record);

	// Collision 필드가 없는 기존 씬을 불러올 때는 현재 설정을 유지한다.
	auto CollisionSlot = Record.TryEnterField("Collision", true);
	if (!CollisionSlot.IsSet())
		return;

	auto CollisionRecord = CollisionSlot.GetValue().EnterRecord();
	const auto SerializeByte = [&CollisionRecord](const char* Name, uint8& Value)
		{
			auto Slot = CollisionRecord.TryEnterField(Name, true);
			if (Slot.IsSet())
				Slot.GetValue() << Value;
		};

	uint8 Enabled = static_cast<uint8>(CollisionEnabled);
	SerializeByte("Enabled", Enabled);
	if (Enabled <= static_cast<uint8>(ECollisionEnabled::QueryOnly))
		CollisionEnabled = static_cast<ECollisionEnabled>(Enabled);

	uint8 Channel = static_cast<uint8>(ObjectType);
	SerializeByte("ObjectType", Channel);
	if (Channel < static_cast<uint8>(ECollisionChannel::Count))
		ObjectType = static_cast<ECollisionChannel>(Channel);

	constexpr std::array<const char*, CollisionChannelCount> ResponseFields{
		"WorldStatic", "WorldDynamic", "Pawn", "Projectile"
	};
	for (size_t i = 0; i < CollisionChannelCount; ++i)
	{
		uint8 Response = static_cast<uint8>(CollisionResponses[i]);
		SerializeByte(ResponseFields[i], Response);
		if (Response <= static_cast<uint8>(ECollisionResponse::Block))
			CollisionResponses[i] = static_cast<ECollisionResponse>(Response);
	}
}

void UPrimitiveComponent::SetCastShadow(bool NewValue)
{
	if (bCastShadow != NewValue)
	{
		bCastShadow = NewValue;
	}
}

void UPrimitiveComponent::SetReceiveShadow(bool NewValue)
{
	if (bReceiveShadow != NewValue)
	{
		bReceiveShadow = NewValue;
	}
}

void UPrimitiveComponent::CreateRenderState()
{
	Super::CreateRenderState();
	GetWorld()->GetScene().AddPrimitive(this);
}

void UPrimitiveComponent::DestroyRenderState()
{
	GetWorld()->GetScene().RemovePrimitive(this);
	Super::DestroyRenderState();
}

bool UPrimitiveComponent::TraceMesh(const FRay& WorldRay, const FStaticMeshData& Mesh, const FMatrix& WorldMatrix, FHitResult& OutResult)
{
	FBox MeshBox = Mesh.AABB.GetWorldAABB(WorldMatrix);
	float BoxT;
	if (!RayIntersectsAABB(WorldRay, MeshBox.Min, MeshBox.Max, BoxT))
	{
		return false;
	}

	FRay LocalRay = ToLocalRay(WorldRay, WorldMatrix);
	float T = FLT_MAX;

	if (!RayIntersectsMesh(LocalRay, Mesh, T)) return false;

	OutResult.HitComponent = this;
	OutResult.Distance = T;
	OutResult.ImpactPoint = WorldRay.Origin + WorldRay.Direction * T;

	return true;
}

bool UPrimitiveComponent::TraceMeshLocal(const FRay& LocalRay, const FStaticMeshData& Mesh, float& OutT)
{
	return RayIntersectsMesh(LocalRay, Mesh, OutT);
}

ECollisionResponse UPrimitiveComponent::GetCollisionResponseToChannel(ECollisionChannel Channel) const
{
	const size_t Index = static_cast<size_t>(Channel);

	if (Index >= CollisionResponses.size())
		return ECollisionResponse::Ignore;

	return CollisionResponses[Index];
}

void UPrimitiveComponent::SetCollisionResponseToChannel(ECollisionChannel Channel, ECollisionResponse Response)
{
	const size_t Index = static_cast<size_t>(Channel);

	if (Index >= CollisionResponses.size())
		return;

	CollisionResponses[Index] = Response;
}

void UPrimitiveComponent::SetCollisionResponseToAllChannels(ECollisionResponse Response)
{
	CollisionResponses.fill(Response);
}
