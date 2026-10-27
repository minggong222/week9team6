#pragma once

#include "Core/Types.h"

struct FRenderStats
{
	uint32 TotalPrimitives = 0;
	uint32 VisiblePrimitives = 0;
	uint32 DrawCalls = 0;
	uint64 Triangles = 0;
	uint32 LODCounts[4] = {};
	uint64 LODTriangles[4] = {};

	uint32 DirectionalCount = 0;
	uint32 PointCount = 0;
	uint32 SpotCount = 0;
	uint32 RectCount = 0;
	float AtlasCapacity = 0.0f;
	float ShadowTiles = 0.0f;
	uint32 ShadowViewsRedrawn = 0; // 이번 프레임에 다시 그린 그림자 뷰(큐브 면, cascade) 수
	uint32 ShadowViewsCached = 0;  // 캐시를 재사용해 건너뛴 그림자 뷰 수
	uint32 ShadowLightsSkippedOutsideView = 0;
	uint32 ShadowLightsSkippedNoReceiver = 0;
	uint32 ShadowLightsSkippedNoCaster = 0;
	uint32 ShadowCastersSkippedSmall = 0;

	uint32 CascadeViewsRedrawn = 0;
	uint32 CascadeViewsCached = 0;

	void Reset() { *this = FRenderStats(); }
};

