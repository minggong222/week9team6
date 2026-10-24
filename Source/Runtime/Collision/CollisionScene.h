#pragma once

#include "Components/ShapeComponent.h"
#include "Math/BVH.h"

class FCollisionScene
{
public:
    void AddShape(UShapeComponent* Shape);
    void RemoveShape(UShapeComponent* Shape);

    void BuildBVH();

    // 구성 변경 시 Build, 위치·크기 변경 시 Refit으로 BVH를 갱신한다.
    void UpdateBVH();

    void MarkBoundsDirty() { bBoundsDirty = true; }

    // BVH에서 AABB가 겹치는 충돌 후보를 찾는다. 실제 형상은 검사하지 않는다.
    void QueryCandidates(UShapeComponent* Shape, TArray<UShapeComponent*>& OutCandidates);

    // Broad Phase 후보에 정밀 판정을 적용해 실제 겹치는 Shape를 찾는다.
    // 선택적으로 후보 목록과 Narrow Phase 검사 횟수도 반환한다.
    void QueryOverlaps(UShapeComponent* Shape, TArray<UShapeComponent*>& OutOverlaps, TArray<UShapeComponent*>* OutCandidates = nullptr, int32* OutNarrowTests = nullptr);

    // 충돌 후보의 응답 설정과 실제 교차 여부를 확인해 Overlap / Block 결과를 분류한다.
    void QueryCollisionInteractions(UShapeComponent* Shape, TArray<UShapeComponent*>& OutOverlaps, TArray<UShapeComponent*>& OutBlocks);

    // 이전 프레임과 현재 Overlap 쌍을 비교해 진입·이탈을 판별한다.
    void UpdateOverlapPairs();

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

    // Sphere / Box / Capsule의 모든 조합에 대해 실제 교차 여부를 판정한다.
    static bool IntersectsShapes(UShapeComponent* A, UShapeComponent* B, int32* OutNarrowTests = nullptr);

    // A-B와 B-A를 동일한 충돌 쌍으로 취급한다.
    struct FCollisionPair
    {
        UShapeComponent* A = nullptr;
        UShapeComponent* B = nullptr;

        bool operator==(const FCollisionPair& Other) const
        {
            return (A == Other.A && B == Other.B) || (A == Other.B && B == Other.A);
        }
    };

    // 두 컴포넌트에 BeginOverlap 또는 EndOverlap을 호출한다.
    // bBeginOverlap이 true이면 BeginOverlap, false이면 EndOverlap을 호출한다.
    void BroadcastOverlapEvent(const FCollisionPair& Pair, bool bBeginOverlap);
    TArray<FCollisionPair> ActiveOverlapPairs;
};
