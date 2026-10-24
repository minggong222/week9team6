#pragma once

#include "Core/Types.h"
#include <cstddef>

enum class ECollisionEnabled : uint8
{
    NoCollision,
    QueryOnly
};

enum class ECollisionResponse : uint8
{
    Ignore,
    Overlap,
    Block
};

enum class ECollisionChannel : uint8
{
    WorldStatic,
    WorldDynamic,
    Pawn,
    Projectile,

    // Count: enum의 개수를 알려주는 항목. 항상 마지막에 둘 것.
    Count
};

constexpr std::size_t CollisionChannelCount = static_cast<std::size_t>(ECollisionChannel::Count);

inline ECollisionResponse ResolveCollisionResponse(ECollisionResponse A, ECollisionResponse B)
{
    if (A == ECollisionResponse::Ignore || B == ECollisionResponse::Ignore)
        return ECollisionResponse::Ignore;

    if (A == ECollisionResponse::Overlap || B == ECollisionResponse::Overlap)
        return ECollisionResponse::Overlap;

    return ECollisionResponse::Block;
}