#pragma once

#include "Components/ShapeComponent.h"
#include "Math/BVH.h"

class FCollisionScene
{
public:
    void AddShape(UShapeComponent* Shape);
    void RemoveShape(UShapeComponent* Shape);

    void BuildBVH();
    void UpdateBVH();

    void MarkBoundsDirty() { bBoundsDirty = true; }

    void QueryCandidates(UShapeComponent* Shape, TArray<UShapeComponent*>& OutCandidates);
    void QueryOverlaps(UShapeComponent* Shape, TArray<UShapeComponent*>& OutOverlaps, TArray<UShapeComponent*>* OutCandidates = nullptr, int32* OutNarrowTests = nullptr);

private:
    TArray<UShapeComponent*> Shapes;

    TBVH<UShapeComponent*> BVH{
        [](UShapeComponent* const& Shape) -> FBox
        {
            return Shape->CalcBounds();
        }
    };

    bool bElementListChanged = false;
    bool bBoundsDirty = false;

    static bool IntersectsShapes(UShapeComponent* A, UShapeComponent* B, int32* OutNarrowTests = nullptr);
};