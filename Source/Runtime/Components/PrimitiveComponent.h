#pragma once

#include "SceneComponent.h"
#include "../Render/Shader.h"
#include "../Render/Mesh.h"
#include "Render/RenderPacket.h"
#include "Render/GeometryGenerator.h"
#include "Core/Delegates/Delegate.h"
#include "Collision/HitResult.h"
#include "Collision/CollisionTypes.h"

#include <array>

class FPrimitiveSceneProxy;

enum class EPrimitiveType
{
	Sphere,
	Cube,
	Cone,
	Plane,

	Particle,
	ParticleQuad
};

class UPrimitiveComponent :public USceneComponent
{
	DECLARE_CLASS(UPrimitiveComponent, USceneComponent)

	REFLECT_START(ClassName)
		PROPERTY(bCastShadow)
		PROPERTY(bReceiveShadow)
		PROPERTY(bGenerateOverlapEvents)
		PROPERTY(bBlockComponent)
	REFLECT_END()

public:
	UPrimitiveComponent();
	virtual ~UPrimitiveComponent();

	virtual void BeginPlay() override;

	// Todo: subuv
	//void SubmitToRenderQueue(TQueue<FRenderPacket>& RenderQueue);
	virtual void SubmitToRenderQueue(FRenderQueue& RenderQueue, const FViewContext& ViewContext);

	virtual const FStaticMeshData* GetMeshData() const { return nullptr; }

	// Outline처럼 컴포넌트를 통째로 한 번 더 그릴 때 쓰는 GPU 메시.
	// 메시가 없거나(텍스트) View에 따라 형상이 정해지는 컴포넌트(빌보드·파티클)는
	// 컴포넌트 월드 행렬만으로 같은 그림을 못 만들므로 nullptr을 돌려준다.
	virtual UStaticMesh* GetRenderMesh() const { return nullptr; }

	virtual int32 GetNumMaterials() const { return 0; }
	virtual UMaterial* GetMaterial(int32 SlotIndex) const { return nullptr; }
	virtual void SetMaterial(int32 SlotIndex, UMaterial* InMaterial) {}
	//
	// FShader* GetShader() const { return Shader.get(); };

	void SetVisible(bool bInVisible) { SetVisibility(bInVisible); }

	virtual bool LineTraceComponent(const FRay& WorldRay, FHitResult& OutHit);
	virtual bool LineTraceComponentLocal(const FRay& LocalRay, float& OutT);
	virtual FBox CalcLocalBounds() const override
	{
		const FStaticMeshData* Data = GetMeshData();
		return Data ? Data->AABB : Super::CalcLocalBounds();
	}

	FPrimitiveSceneProxy* GetSceneProxy() const { return SceneProxy; }

	void MarkRenderStateDirty() override;

	virtual void OnTransformDirty() override;

	virtual bool MoveComponent(const FVector& Delta, const FRotator& NewRotation, bool bSweep, FHitResult* Hit = nullptr) override;

	virtual void PostEditChangeProperty(const FProperty& Property) override;
	using Super::Serialize;
	void Serialize(FStructuredArchive::FRecord Record) override;

	void SetCastShadow(bool NewValue);
	void SetReceiveShadow(bool NewValue);

	bool bCastShadow = true;
	bool bReceiveShadow = true;

	ECollisionEnabled GetCollisionEnabled() const { return CollisionEnabled; }
	void SetCollisionEnabled(ECollisionEnabled InEnabled) { CollisionEnabled = InEnabled; }

	ECollisionChannel GetCollisionObjectType() const { return ObjectType; }
	void SetCollisionObjectType(ECollisionChannel InChannel) { ObjectType = InChannel; }

	bool GetGenerateOverlapEvents() const { return bGenerateOverlapEvents; }
	void SetGenerateOverlapEvents(bool bEnabled) { bGenerateOverlapEvents = bEnabled; }

	ECollisionResponse GetCollisionResponseToChannel(ECollisionChannel Channel) const;
	void SetCollisionResponseToChannel(ECollisionChannel Channel, ECollisionResponse Response);
	void SetCollisionResponseToAllChannels(ECollisionResponse Response);

	// 다른 컴포넌트와 겹치기 시작하거나 겹침이 끝났을 때 발생하는 이벤트
	DECLARE_DELEGATE(OnComponentBeginOverlap, UPrimitiveComponent*);
	DECLARE_DELEGATE(OnComponentEndOverlap, UPrimitiveComponent*);

protected:
	virtual bool ShouldCreateRenderState() const override
	{
		return true;
	}

	virtual void CreateRenderState() override;
	virtual void DestroyRenderState() override;

	bool TraceMesh(const FRay& WorldRay, const FStaticMeshData& Mesh, const FMatrix& WorldMatrix, FHitResult& OutResult);
	bool TraceMeshLocal(const FRay& LocalRay, const FStaticMeshData& Mesh, float& OutT);

private:
	friend class FScene;
	FPrimitiveSceneProxy* SceneProxy = nullptr;

	bool bGenerateOverlapEvents = true;
	bool bBlockComponent = true;

	ECollisionEnabled CollisionEnabled = ECollisionEnabled::QueryOnly;
	ECollisionChannel ObjectType = ECollisionChannel::WorldDynamic;
	std::array<ECollisionResponse, CollisionChannelCount> CollisionResponses;
};
