#include "EnginePCH.h"
#include "RenderPacket.h"
#include "Material.h"
#include "Mesh.h"

#include <bit>

uint64 FRenderQueue::MakeSortKey(const FRenderPacket& Packet)
{
	if (Packet.Material->PSOType != EPSOType::StaticMesh_Opaque)
	{
		const uint64 DistanceBits = static_cast<uint64>(std::bit_cast<uint32>(Packet.CameraToParticleDistance));
		return (1ull << 63) | (~DistanceBits & 0xFFFFFFFFull);
	}

	return (static_cast<uint64>(Packet.Material->SortID) << 47)
		| (static_cast<uint64>(Packet.Mesh->SortID) << 31)
		| (static_cast<uint64>(Packet.LODIndex & 0x3) << 29);
}
