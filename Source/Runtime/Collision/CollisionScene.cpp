#include "EnginePCH.h"
#include "CollisionScene.h"

#include "CollisionGeometry.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"

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

bool FCollisionScene::IntersectsShapes(UShapeComponent* A, UShapeComponent* B, int32* OutNarrowTests)
{
    USphereComponent* SphereA = Cast<USphereComponent>(A);
    USphereComponent* SphereB = Cast<USphereComponent>(B);

    if (SphereA && SphereB)
    {
        if (OutNarrowTests)
            ++(*OutNarrowTests);

        return FCollisionGeometry::IntersectsSphereSphere(
            SphereA->GetWorldLocation(), SphereA->GetScaledSphereRadius(),
            SphereB->GetWorldLocation(), SphereB->GetScaledSphereRadius());
    }

    return false;
}