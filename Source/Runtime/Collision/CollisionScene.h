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
};