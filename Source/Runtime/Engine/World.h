#pragma once

#include "UObject/Object.h"
#include "UObject/Class.h"
#include "GameFramework/Actor.h"
#include "Collision/CollisionScene.h"
#include "Components/PrimitiveComponent.h"
#include "Math/Transform.h"
#include "Render/Renderer.h"
#include "PathTracker.h"
#include "Math/Frustum.h"
#include "Engine/Scene.h"
#include "Camera/CameraActor.h"
#include "Engine/EngineTypes.h"
#include "Render/SceneRenderData.h"

class ULevel;
class UGameInstance;
class FLuaManager;
class UBillboardComponent;
class APawn;
class APlayerController;

struct FViewContext;

class UWorld : public UObject
{
	DECLARE_CLASS(UWorld, UObject)

public:

	////////////////////////////////////////////////////////////
	// 월드 생명주기
	////////////////////////////////////////////////////////////

	bool Init();		// Initialize
	void ClearWorld();	// Release

	void BeginPlay();
	void Tick(float DeltaTime);
	void EndPlay();

	bool HasBegunPlay() const { return bBegunPlay; }

	bool IsPaused() const { return bIsPaused; }
	void SetPaused(bool bPaused);

	// 배율은 다음 월드 Tick부터 적용하며, 하위 Tick에는 계산된 게임 시간을 전달한다.
	float GetGlobalTimeDilation() const { return GlobalTimeDilation; }
	void SetGlobalTimeDilation(float Value);
	float GetDeltaSeconds() const { return DeltaSeconds; }
	// EngineTimer에서 전달받은 배율 적용 전 시간으로, 프레임 시간 제한이 포함될 수 있다.
	float GetRealDeltaSeconds() const { return RealDeltaSeconds; }


	////////////////////////////////////////////////////////////
	// 월드 생성 및 유형
	////////////////////////////////////////////////////////////

	static UWorld* CreateWorld(const EWorldType InWorldType, bool bInformEngineOfWorld, FName WorldName = NAME_None);

	EWorldType GetWorldType() const { return WorldType; }
	void SetWorldType(EWorldType InType) { WorldType = InType; }

	bool IsPlayInEditor() const { return WorldType == EWorldType::PIE; }


	////////////////////////////////////////////////////////////
	// 게임 인스턴스 및 플레이어
	////////////////////////////////////////////////////////////

	void SetGameInstance(UGameInstance* InGameInstance) { OwningGameInstance = InGameInstance; }
	UGameInstance* GetGameInstance() const { return OwningGameInstance; }
	FLuaManager* GetLuaManager() const;

	APawn* GetPlayerPawn() const { return PlayerPawn; }
	APlayerController* GetPlayerController() const { return PlayerController; }


	////////////////////////////////////////////////////////////
	// 레벨
	////////////////////////////////////////////////////////////

	ULevel* GetPersistentLevel() const { return PersistentLevel; }
	const TArray<ULevel*>& GetLevels() const { return Levels; }
	void SetPersistentLevel(ULevel* InLevel) { PersistentLevel = InLevel; }

	ULevel* GetCurrentLevel() const { return CurrentLevel; }
	void SetCurrentLevel(ULevel* InLevel) { CurrentLevel = InLevel; }


	////////////////////////////////////////////////////////////
	// 액터 및 컴포넌트
	////////////////////////////////////////////////////////////

	AActor* SpawnActor(UClass* Class, FName InName = NAME_None, const FTransform* Transform = nullptr);

	template <class T>
	T* SpawnActor(FName InName = NAME_None, const FTransform* Transform = nullptr)
	{
		return CastChecked<T>(SpawnActor(T::StaticClass(), InName, Transform));
	}

	int32 GetActorNum();
	bool DestroyActor(AActor* Actor);
	bool IsActorInWorld(const AActor* Actor, uint32 ExpectedUUID) const;

	// 등록은 즉시, 초기화와 BeginPlay는 다음 시작 배치에서 실행한다.
	void EnqueueComponentStart(UActorComponent* Component);


	////////////////////////////////////////////////////////////
	// 카메라
	////////////////////////////////////////////////////////////

	void CreateMainCamera();

	void SetMainCamera(ACameraActor* Camera);
	ACameraActor* GetMainCamera() const { return MainCamera; }
	UCameraComponent* GetMainCameraComponent() const;
	UCameraComponent* GetPlayerCamera() const;

	// 게임 뷰포트 카메라를 선택한다. nullptr이면 자동 선택으로 복귀한다.
	// 다른 월드의 카메라이거나 게임 뷰포트가 없으면 false를 반환한다.
	bool SetMainCameraComponent(UCameraComponent* Camera, float BlendTime = 0.0f);


	////////////////////////////////////////////////////////////
	// 렌더링 및 씬
	////////////////////////////////////////////////////////////

	void GatherRenderPackets(FRenderQueue& RenderArray, const FViewContext* LODView = nullptr, const FFrustumPlanes* Frustum = nullptr, FRenderer* Renderer = nullptr);
	FScene& GetScene() { return Scene; }
	const FRenderStats& GetRenderStats() const { return RenderStats; }


	////////////////////////////////////////////////////////////
	// 충돌 및 레이 트레이스
	////////////////////////////////////////////////////////////

	FCollisionScene& GetCollisionScene() { return CollisionScene; }

	// View별 Billboard 행렬 공급자는 이 동기 호출 동안만 사용하며 저장하지 않는다.
	using FBillboardTraceTransform = FMatrix(*)(const UBillboardComponent&, const void*);

	// 현재 World의 Component에 Ray를 전달하고 가장 가까운 유효 교차를 반환한다.
	bool LineTraceSingle(const FRay& WorldRay, FHitResult& OutHit,
		FBillboardTraceTransform ResolveBillboard = nullptr, const void* ViewContext = nullptr);
	bool SweepSingle(const FRay& WorldRay, float MaxDistance, float Radius, FHitResult& OutHit, AActor* IgnoreActor = nullptr, ECollisionChannel QueryChannel = ECollisionChannel::WorldDynamic, bool bRespectCollisionResponse = false);

	bool SweepCameraSphere(const FRay& WorldRay, float MaxDistance, float Radius, FHitResult& OutHit, AActor* IgnoreActor = nullptr);


	////////////////////////////////////////////////////////////
	// 틱 및 경로 추적
	////////////////////////////////////////////////////////////

	FTickTaskManager& GetTickTaskManager() { return TickTaskManager; }
	FPathTracker& GetPathTracker() { return PathTracker; }


	////////////////////////////////////////////////////////////
	// 복제 및 직렬화
	////////////////////////////////////////////////////////////

	static UWorld* GetDuplicatedWorldForPIE(UWorld* InWorld);
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	virtual void Serialize(FStructuredArchive::FRecord Record) override;

private:
	// 컴포넌트 시작 및 지연 삭제 처리
	void ProcessPendingStarts();

	struct FPendingComponentStart
	{
		AActor* Owner = nullptr;
		uint32 OwnerUUID = 0;
		UActorComponent* Component = nullptr;
		uint32 ComponentUUID = 0;
	};

	TArray<FPendingComponentStart> PendingComponentStarts;
	bool bIsProcessingStarts = false;

	void ProcessPendingDestroyActors();
	void ProcessPendingComponentDestructions();
	bool bIsProcessingPendingComponentDestruction = false;

	struct FPendingActorDestroy
	{
		AActor* Actor = nullptr;
		uint32 UUID = 0;
	};

	// 월드 소유자 및 유형
	UGameInstance* OwningGameInstance = nullptr;
	EWorldType WorldType = EWorldType::None;

	// 병렬 렌더 패킷 수집에 사용하는 청크별 버퍼
	struct alignas(64) FGatherChunk
	{
		TArray<FRenderPacket> Packets;              // 스태틱 묶음에 못 들어가는 것 (반투명 섹션, Renderer 없는 호출)
		TArray<uint32> SlowPathIndices;
		std::vector<FStaticDrawGroup> Groups;       // 불투명 스태틱 메시: (머티리얼, 메시, LOD)별 묶음. 항목만 매 프레임 비운다
		uint32 LastGroup = 0;                       // 바로 전 물체가 들어간 묶음 (연속한 물체는 대개 같은 묶음)
		uint32 StaticDrawCount = 0;
		uint32 LODCounts[4] = {};
		uint64 LODTriangles[4] = {};
	};

	TArray<FGatherChunk> GatherChunks;

	// 등록된 Tick 함수만 실행한다. Actor보다 먼저 사라져도 남은 함수와의 연결을 스스로 끊는다.
	FTickTaskManager TickTaskManager;

	// 액터의 플레이 시작 및 지연 삭제 대기열
	TQueue<AActor*> BeginPlayList;
	TQueue<FPendingActorDestroy> PendingDestroyList;

	// 메인 카메라 및 플레이어 상태
	ACameraActor* MainCamera = nullptr;
	APawn* PlayerPawn = nullptr;
	APlayerController* PlayerController = nullptr;
	bool bIsPaused = false;
	bool bBegunPlay = false;

	// 월드별 실행 상태이며, 플레이 종료 시 기본값으로 복원한다.
	float GlobalTimeDilation = 1.0f;
	float DeltaSeconds = 0.0f;
	float RealDeltaSeconds = 0.0f;

	// 경로 추적
	FPathTracker PathTracker;

	// 월드가 관리하는 레벨
	ULevel* PersistentLevel = nullptr;
	ULevel* CurrentLevel = nullptr;
	TArray<ULevel*> Levels;

	// 렌더링 및 충돌 씬
	FScene Scene;
	FCollisionScene CollisionScene;

	// GatherRenderPackets가 매 프레임 채우는 컬링 결과. 용량을 재사용한다.
	TArray<FPrimitiveSceneProxy*> VisibleProxies;
	TArray<FLODSelectionInput> LODInputs;
	TArray<uint8> SelectedLODs;

	FRenderStats RenderStats;

	// 종료 및 정리 중 재진입 방지 상태
	bool bIsEndingPlay = false;
	bool bIsClearingWorld = false;
};
