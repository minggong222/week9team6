#include "EnginePCH.h"
#include "CollisionScene.h"

#include "CollisionGeometry.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"

#include "GameFramework/Actor.h"

namespace
{
    // 충돌 활성 상태와 양쪽 채널 응답을 종합해 Ignore / Overlap / Block을 결정한다.
    // 실제로 겹쳤는지는 검사하지 않는다.
    ECollisionResponse GetPairCollisionResponse(const UShapeComponent* A, const UShapeComponent* B)
    {
        if (!A || !B)
            return ECollisionResponse::Ignore;

        if (A->GetCollisionEnabled() == ECollisionEnabled::NoCollision ||
            B->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
            return ECollisionResponse::Ignore;

        const AActor* OwnerA = A->GetOwner();
        const AActor* OwnerB = B->GetOwner();

        if (!OwnerA || !OwnerB || OwnerA == OwnerB || !OwnerA->GetActorEnableCollision() || !OwnerB->GetActorEnableCollision())
            return ECollisionResponse::Ignore;

        return ResolveCollisionResponse(
            A->GetCollisionResponseToChannel(B->GetCollisionObjectType()),
            B->GetCollisionResponseToChannel(A->GetCollisionObjectType()));
    }
}

void FCollisionScene::AddShape(UShapeComponent* Shape)
{
    if (!Shape || Shapes.Find(Shape) != INDEX_NONE)
        return;

    Shapes.Add(Shape);
    bElementListChanged = true;
}

void FCollisionScene::RemoveShape(UShapeComponent* Shape)
{
    if (!Shape)
        return;

    const int32 Index = Shapes.Find(Shape);
    if (Index == INDEX_NONE)
        return;

    Shapes.RemoveAtSwap(Index);
    bElementListChanged = true;

    // EndOverlap에서 다시 삭제를 요청해도 같은 이벤트가 반복되지 않도록
    // 해당 Shape가 포함된 쌍을 활성 목록에서 먼저 제거한다.
    TArray<FCollisionPair> EndPairs;
    for (int32 i = ActiveOverlapPairs.Num() - 1; i >= 0; --i)
    {
        const FCollisionPair& Pair = ActiveOverlapPairs[i];

        if (Pair.A == Shape || Pair.B == Shape)
        {
            EndPairs.Add(Pair);
            ActiveOverlapPairs.RemoveAtSwap(i);
        }
    }

    for (const FCollisionPair& Pair : EndPairs)
    {
        UShapeComponent* Other = Pair.A == Shape ? Pair.B : Pair.A;
        if (Shapes.Find(Other) != INDEX_NONE)
            BroadcastOverlapEvent(Pair, false);
    }
}

void FCollisionScene::BuildBVH()
{
    BVH.Build(std::span(Shapes.GetData(), Shapes.Num()));
}

void FCollisionScene::UpdateBVH()
{
    if (bElementListChanged)
    {
        BuildBVH();
        bElementListChanged = false;
        bBoundsDirty = false;
    }
    else if (bBoundsDirty)
    {
        BVH.Refit();
        bBoundsDirty = false;
    }
}

void FCollisionScene::QueryCandidates(UShapeComponent* Shape, TArray<UShapeComponent*>& OutCandidates)
{
    OutCandidates.Reset();

    if (!Shape)
        return;

    UpdateBVH();

    const FBox QueryBounds = Shape->CalcBounds();

    BVH.Query(
        [&](const FBox& Bounds)
        {
            return FCollisionGeometry::IntersectsAABB(QueryBounds, Bounds);
        },
        [&](UShapeComponent* Candidate)
        {
            if (Candidate != Shape)
                OutCandidates.Add(Candidate);
        }
    );
}

void FCollisionScene::QueryOverlaps(UShapeComponent* Shape, TArray<UShapeComponent*>& OutOverlaps, TArray<UShapeComponent*>* OutCandidates, int32* OutNarrowTests)
{
    OutOverlaps.Reset();

    if (OutNarrowTests)
        *OutNarrowTests = 0;

    if (!Shape)
        return;

    TArray<UShapeComponent*> Candidates;
    QueryCandidates(Shape, Candidates);

    if (OutCandidates)
        *OutCandidates = Candidates;

    for (UShapeComponent* Candidate : Candidates)
    {
        if (IntersectsShapes(Shape, Candidate, OutNarrowTests))
            OutOverlaps.Add(Candidate);
    }
}

void FCollisionScene::QueryCollisionInteractions(UShapeComponent* Shape, TArray<UShapeComponent*>& OutOverlaps, TArray<UShapeComponent*>& OutBlocks)
{
    OutOverlaps.Reset();
    OutBlocks.Reset();

    if (!Shape)
        return;

    TArray<UShapeComponent*> Candidates;
    QueryCandidates(Shape, Candidates);

    for (UShapeComponent* Candidate : Candidates)
    {
        const ECollisionResponse Response = GetPairCollisionResponse(Shape, Candidate);

        if (Response == ECollisionResponse::Ignore || !IntersectsShapes(Shape, Candidate))
            continue;

        if (Response == ECollisionResponse::Overlap)
            OutOverlaps.Add(Candidate);
        else
            OutBlocks.Add(Candidate);
    }
}

void FCollisionScene::UpdateOverlapPairs()
{
    TArray<FCollisionPair> CurrentPairs;

    for (UShapeComponent* Shape : Shapes)
    {
        if (!Shape->GetGenerateOverlapEvents())
            continue;

        TArray<UShapeComponent*> Overlaps;
        TArray<UShapeComponent*> Blocks;

        QueryCollisionInteractions(Shape, Overlaps, Blocks);

        for (UShapeComponent* Other : Overlaps)
        {
            if (!Other->GetGenerateOverlapEvents())
                continue;

            FCollisionPair Pair{ Shape, Other };

            if (CurrentPairs.Find(Pair) == INDEX_NONE)
                CurrentPairs.Add(Pair);
        }
    }

    TArray<FCollisionPair> BeginPairs;
    TArray<FCollisionPair> EndPairs;

    // 새로 겹친 쌍
    for (const FCollisionPair& Pair : CurrentPairs)
    {
        if (ActiveOverlapPairs.Find(Pair) == INDEX_NONE)
            BeginPairs.Add(Pair);
    }

    // 겹침이 끝난 쌍
    for (const FCollisionPair& Pair : ActiveOverlapPairs)
    {
        if (CurrentPairs.Find(Pair) == INDEX_NONE)
            EndPairs.Add(Pair);
    }

    // 이벤트 발생 전에 이전 상태를 갱신한다.
    ActiveOverlapPairs = std::move(CurrentPairs);

    // 포인터를 역참조하기 전에 두 Shape가 아직 등록되어 있는지 확인한다.
    const auto IsPairRegistered = [this](const FCollisionPair& Pair)
        {
            return Shapes.Find(Pair.A) != INDEX_NONE && Shapes.Find(Pair.B) != INDEX_NONE;
        };

    for (const FCollisionPair& Pair : BeginPairs)
    {
        if (!IsPairRegistered(Pair))
            continue;

        BroadcastOverlapEvent(Pair, true);
    }

    for (const FCollisionPair& Pair : EndPairs)
    {
        if (!IsPairRegistered(Pair))
            continue;

        BroadcastOverlapEvent(Pair, false);
    }
}

void FCollisionScene::BroadcastOverlapEvent(const FCollisionPair& Pair, bool bBeginOverlap)
{
    AActor* OwnerA = Pair.A->GetOwner();
    AActor* OwnerB = Pair.B->GetOwner();
    const bool bWasExecutingA = OwnerA && OwnerA->bIsExecutingComponentCallback;
    const bool bWasExecutingB = OwnerB && OwnerB->bIsExecutingComponentCallback;

    // 콜백에서 요청한 Actor/Component 삭제는 이벤트 처리가 끝날 때까지 미룬다.
    if (OwnerA) OwnerA->bIsExecutingComponentCallback = true;
    if (OwnerB) OwnerB->bIsExecutingComponentCallback = true;

    if (bBeginOverlap)
    {
        Pair.A->OnComponentBeginOverlap.Broadcast(Pair.B);

        if (Shapes.Find(Pair.A) != INDEX_NONE && Shapes.Find(Pair.B) != INDEX_NONE)
            Pair.B->OnComponentBeginOverlap.Broadcast(Pair.A);
    }
    else
    {
        Pair.A->OnComponentEndOverlap.Broadcast(Pair.B);
        Pair.B->OnComponentEndOverlap.Broadcast(Pair.A);
    }

    // 바깥쪽 콜백이 실행 중일 수 있으므로 저장해 둔 상태를 복원한다.
    if (OwnerA) OwnerA->bIsExecutingComponentCallback = bWasExecutingA;
    if (OwnerB) OwnerB->bIsExecutingComponentCallback = bWasExecutingB;
}

void FCollisionScene::BroadcastHitEvent(UShapeComponent* MovedShape, UShapeComponent* OtherShape)
{
    if (!MovedShape || !OtherShape ||
        Shapes.Find(MovedShape) == INDEX_NONE || Shapes.Find(OtherShape) == INDEX_NONE)
        return;

    AActor* OwnerA = MovedShape->GetOwner();
    AActor* OwnerB = OtherShape->GetOwner();

    if (!OwnerA || !OwnerB)
        return;

    const bool bWasExecutingA = OwnerA->bIsExecutingComponentCallback;
    const bool bWasExecutingB = OwnerB->bIsExecutingComponentCallback;

    // 이벤트 실행 도중 Actor/Component 삭제 요청을 지연시킨다.
    OwnerA->bIsExecutingComponentCallback = true;
    OwnerB->bIsExecutingComponentCallback = true;

    MovedShape->OnComponentHit.Broadcast(OtherShape);

    if (Shapes.Find(MovedShape) != INDEX_NONE && Shapes.Find(OtherShape) != INDEX_NONE)
        OtherShape->OnComponentHit.Broadcast(MovedShape);

    OwnerA->bIsExecutingComponentCallback = bWasExecutingA;
    OwnerB->bIsExecutingComponentCallback = bWasExecutingB;
}

bool FCollisionScene::IntersectsShapes(UShapeComponent* A, UShapeComponent* B, int32* OutNarrowTests)
{
    USphereComponent* SphereA = Cast<USphereComponent>(A);
    USphereComponent* SphereB = Cast<USphereComponent>(B);
    UBoxComponent* BoxA = Cast<UBoxComponent>(A);
    UBoxComponent* BoxB = Cast<UBoxComponent>(B);
    UCapsuleComponent* CapsuleA = Cast<UCapsuleComponent>(A);
    UCapsuleComponent* CapsuleB = Cast<UCapsuleComponent>(B);

    if ((!SphereA && !BoxA && !CapsuleA) || (!SphereB && !BoxB && !CapsuleB))
        return false;

    // 순서만 반대인 조합도 같은 판정 코드를 쓰도록 인자를 바꾼다.
    if ((!SphereA && SphereB) || (BoxA && CapsuleB))
        return IntersectsShapes(B, A, OutNarrowTests);

    if (OutNarrowTests)
        ++(*OutNarrowTests);

    if (SphereA && SphereB)
    {
        return FCollisionGeometry::IntersectsSphereSphere(
            SphereA->GetWorldLocation(), SphereA->GetScaledSphereRadius(),
            SphereB->GetWorldLocation(), SphereB->GetScaledSphereRadius());
    }

    if (SphereA && BoxB)
    {
        return FCollisionGeometry::IntersectsSphereBox(
            SphereA->GetWorldLocation(), SphereA->GetScaledSphereRadius(),
            BoxB->GetWorldLocation(), BoxB->GetWorldRotation(), BoxB->GetScaledBoxExtent());
    }

    if (SphereA && CapsuleB)
    {
        FVector Start, End;
        CapsuleB->GetWorldCapsuleSegment(Start, End);
        return FCollisionGeometry::IntersectsSphereCapsule(
            SphereA->GetWorldLocation(), SphereA->GetScaledSphereRadius(), Start, End, CapsuleB->GetScaledCapsuleRadius());
    }

    if (BoxA && BoxB)
    {
        return FCollisionGeometry::IntersectsBoxBox(
            BoxA->GetWorldLocation(), BoxA->GetWorldRotation(), BoxA->GetScaledBoxExtent(),
            BoxB->GetWorldLocation(), BoxB->GetWorldRotation(), BoxB->GetScaledBoxExtent());
    }

    FVector StartA, EndA;
    CapsuleA->GetWorldCapsuleSegment(StartA, EndA);
    if (CapsuleB)
    {
        FVector StartB, EndB;
        CapsuleB->GetWorldCapsuleSegment(StartB, EndB);
        return FCollisionGeometry::IntersectsCapsuleCapsule(
            StartA, EndA, CapsuleA->GetScaledCapsuleRadius(), StartB, EndB, CapsuleB->GetScaledCapsuleRadius());
    }

    return FCollisionGeometry::IntersectsCapsuleBox(
        StartA, EndA, CapsuleA->GetScaledCapsuleRadius(),
        BoxB->GetWorldLocation(), BoxB->GetWorldRotation(), BoxB->GetScaledBoxExtent());
}

