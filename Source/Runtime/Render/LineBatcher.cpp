#include "EnginePCH.h"
#include "LineBatcher.h"
#include "Components/PrimitiveComponent.h"

#include "RenderCommand.h"
#include "RenderResourceManager.h"

#include "UObject/UObjectIterator.h"

FLineBatcher::~FLineBatcher()
{
	delete[] VertexBufferBase;
}

bool FLineBatcher::Init(FRenderer* InRenderer, UWorld* InWorld)
{
	Renderer = InRenderer;
	World = InWorld;

	LineShader = FRenderResourceManager::GetShaderProgram("Resources/Shader/BoundingBoxShader.hlsl");

	PipelineState.Shader = LineShader;
	PipelineState.Topology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;

	MaxVertices = 65536;
	VertexBufferBase = new FLineBatchVertex[MaxVertices];

	VertexBuffer = RenderCommand::CreateDynamicVertexBuffer(sizeof(FLineBatchVertex) * MaxVertices, sizeof(FLineBatchVertex));

	CB = RenderCommand::CreateConstantBuffer(sizeof(FMatrix));

	return true;
}

void FLineBatcher::BuildVertexBuffer()
{
	for (TObjectIterator<UPrimitiveComponent> Itr; Itr; ++Itr)
	{
		// 로컬 AABB와 월드 행렬을 그대로 넘기고 변환은 AddOrientedBox가 한다.
		// (월드 행렬이어야 부모에 붙은 컴포넌트도 제자리에 그려진다)
		AddOrientedBox(
			Itr->CalcLocalBounds(),
			Itr->GetWorldMatrix(),
			FVector4(1.0f, 1.0f, 1.0f, 1.0f));
	}
}

void FLineBatcher::BeginFrame()
{
	VertexBufferPtr = VertexBufferBase;
	VertexCount = 0;
}

void FLineBatcher::AddLine(const FVector& A, const FVector& B, const FVector4& Color)
{
	if (VertexCount + 2 > MaxVertices)
		return;   // 넘치면 버림

	*VertexBufferPtr++ = { A, FVector2(), Color };
	*VertexBufferPtr++ = { B, FVector2(), Color };

	VertexCount += 2;
}


namespace
{
	constexpr int32 CIRCLE_SEGMENT_COUNT = 32;

	void AddWireCircle(FLineBatcher* LineBatcher, const FVector& Center, const FVector& AxisA, const FVector& AxisB, float Radius, const FVector4& Color)
	{
		FVector Previous = Center + AxisA * Radius;

		for (int32 i = 1; i <= CIRCLE_SEGMENT_COUNT; ++i)
		{
			const float Angle = 2.0f * PI * i / CIRCLE_SEGMENT_COUNT;

			const FVector Current = Center + AxisA * (std::cosf(Angle) * Radius) + AxisB * (std::sinf(Angle) * Radius);

			LineBatcher->AddLine(Previous, Current, Color);
			Previous = Current;
		}
	}

	// 박스 모서리 12개. 코너 배열의 인덱스 쌍이며 AABB/OBB가 같은 순서를 쓴다.
	const int BOX_EDGES[12][2] = {
		{0,1},{1,2},{2,3},{3,0},      // 아랫면
		{4,5},{5,6},{6,7},{7,4},      // 윗면
		{0,4},{1,5},{2,6},{3,7},      // 기둥
	};

	// Min/Max로 코너 8개를 만든다. BOX_EDGES가 기대하는 순서다.
	void MakeCorners(const FVector& Min, const FVector& Max, FVector* OutCorners)
	{
		OutCorners[0] = { Min.X, Min.Y, Min.Z };
		OutCorners[1] = { Max.X, Min.Y, Min.Z };
		OutCorners[2] = { Max.X, Max.Y, Min.Z };
		OutCorners[3] = { Min.X, Max.Y, Min.Z };
		OutCorners[4] = { Min.X, Min.Y, Max.Z };
		OutCorners[5] = { Max.X, Min.Y, Max.Z };
		OutCorners[6] = { Max.X, Max.Y, Max.Z };
		OutCorners[7] = { Min.X, Max.Y, Max.Z };
	}
}

void FLineBatcher::AddBox(FBox BoundingBox, FVector4 Color)
{
	FVector C[8];
	MakeCorners(BoundingBox.Min, BoundingBox.Max, C);

	for (const auto& Edge : BOX_EDGES)
	{
		AddLine(C[Edge[0]], C[Edge[1]], Color);
	}
}

void FLineBatcher::AddOrientedBox(const FBox& LocalBox, const FMatrix& WorldMatrix, const FVector4& Color)
{
	FVector C[8];
	MakeCorners(LocalBox.Min, LocalBox.Max, C);

	// 변환 후에 Min/Max를 다시 재면 회전이 뭉개지므로,
	// 코너를 하나씩 옮긴 뒤 원래 순서 그대로 잇는다.
	for (FVector& Corner : C)
	{
		const FVector4 WorldCorner = FVector4(Corner, 1.0f) * WorldMatrix;
		Corner = FVector(WorldCorner.X, WorldCorner.Y, WorldCorner.Z);
	}

	for (const auto& Edge : BOX_EDGES)
	{
		AddLine(C[Edge[0]], C[Edge[1]], Color);
	}

}

void FLineBatcher::AddPath(const TArray<FVector>& Points, const FVector4& Color)
{
	if (Points.IsEmpty())
	{
		return;
	}
	for (size_t i = 0; i < Points.size() - 1; ++i) {
		AddLine(Points[i], Points[i + 1], Color);
	}
}

void FLineBatcher::AddWireSphere(const FVector& Center, float Radius, const FVector4& Color)
{
	if (Radius <= 0.0f)
		return;

	AddWireCircle(this, Center, FVector(1, 0, 0), FVector(0, 1, 0), Radius, Color);
	AddWireCircle(this, Center, FVector(1, 0, 0), FVector(0, 0, 1), Radius, Color);
	AddWireCircle(this, Center, FVector(0, 1, 0), FVector(0, 0, 1), Radius, Color);
}

void FLineBatcher::AddWireCapsule(const FVector& Center, const FVector& Axis, float Radius, float HalfHeight, const FVector4& Color)
{
	if (Radius <= 0.0f || HalfHeight < Radius || Axis.SizeSquared() <= SMALL_NUMBER)
		return;

	const FVector Direction = Axis.Normalized();
	const float SegmentHalfLength = HalfHeight - Radius;

	const FVector Top = Center + Direction * SegmentHalfLength;
	const FVector Bottom = Center - Direction * SegmentHalfLength;

	const FVector Reference = std::abs(Direction.Z) < 0.9f ? FVector(0, 0, 1) : FVector(1, 0, 0);
	const FVector Right = Direction.Cross(Reference).Normalized();
	const FVector Up = Direction.Cross(Right).Normalized();

	// 원통 위아래의 원
	AddWireCircle(this, Top, Right, Up, Radius, Color);
	AddWireCircle(this, Bottom, Right, Up, Radius, Color);

	// 원통 측면의 직선 4개
	const FVector Directions[] = { Right, -Right, Up, -Up };

	for (const FVector& Side : Directions)
	{
		const FVector Start = Top + Side * Radius;
		const FVector End = Bottom + Side * Radius;

		AddLine(Start, End, Color);
	}

	// 위아래 반구의 곡선
	constexpr int32 ARC_SEGMENT_COUNT = 16;
	const FVector ArcAxes[] = { Right, Up };

	for (const FVector& Side : ArcAxes)
	{
		FVector PreviousTop = Top + Side * Radius;
		FVector PreviousBottom = Bottom + Side * Radius;

		for (int32 i = 1; i <= ARC_SEGMENT_COUNT; ++i)
		{
			const float Angle = PI * i / ARC_SEGMENT_COUNT;
			const float CosAngle = std::cosf(Angle);
			const float SinAngle = std::sinf(Angle);

			const FVector CurrentTop = Top + Side * (CosAngle * Radius) + Direction * (SinAngle * Radius);
			const FVector CurrentBottom = Bottom + Side * (CosAngle * Radius) - Direction * (SinAngle * Radius);

			AddLine(PreviousTop, CurrentTop, Color);
			AddLine(PreviousBottom, CurrentBottom, Color);

			PreviousTop = CurrentTop;
			PreviousBottom = CurrentBottom;
		}
	}
}

void FLineBatcher::Flush()
{
	RenderCommand::BindPipelineState(PipelineState);
	RenderCommand::BindVertexBuffer(VertexBuffer.get());
	RenderCommand::BindConstantBuffer(0, CB.get(), EShaderBindFlagBits::Vertex);

	RenderCommand::Draw(VertexCount, 0);
}

void FLineBatcher::OnRender(const FMatrix& ViewProjection)
{
	//BuildVertexBuffer();

	FMatrix VP = ViewProjection;
	RenderCommand::UpdateBufferData(VertexBuffer.get(), VertexBufferBase, sizeof(FLineBatchVertex) * VertexCount);
	RenderCommand::UpdateBufferData(CB.get(), &VP, sizeof(FMatrix));

	Flush();
}

