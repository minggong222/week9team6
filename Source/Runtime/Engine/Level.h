#pragma once

#include "UObject/Object.h"
#include "UObject/Class.h"
#include "Containers/Array.h"

class UWorld;
class AActor;

class ULevel : public UObject
{
    DECLARE_CLASS(ULevel, UObject)

public:
    ULevel() = default;
    virtual ~ULevel() = default;

    UWorld* GetWorld() const { return OwningWorld; }
    void SetWorld(UWorld* InWorld) { OwningWorld = InWorld; }

    const TArray<AActor*>& GetActors() const  { return Actors; }
    uint32 GetActorNum() const { return Actors.Num(); }

    void AddActor(AActor* Actor);
    void ClearActors();

    uint64 GetActorListRevision() const { return ActorListRevision; }
    void MarkActorListDirty() { ++ActorListRevision; }

    using Super::Serialize;   
    virtual void Serialize(FStructuredArchive::FRecord Record) override;

    //virtual void Serialize(FArchive& Ar) override; // Save Level 구현예정

private:
    UWorld* OwningWorld = nullptr;
    TArray<AActor*> Actors;
    // 표시용 캐시가 생성·삭제·이름 변경을 감지한다. 소비하거나 직렬화하지 않는다.
    uint64 ActorListRevision = 0;

    // AWorldSettings* WorldSettings = nullptr;

    bool bIsVisible = true;

    friend class UWorld;
};
