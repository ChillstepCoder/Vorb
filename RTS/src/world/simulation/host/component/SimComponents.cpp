#include "stdafx.h"
#include "SimComponents.h"

bool SimPositionComponent::updatePosition(f32v2 newPosition, ui32 worldWidthChunks) {
    position = newPosition;
    ChunkID newChunk = (newPosition.y / CHUNK_WIDTH) * worldWidthChunks + newPosition.x / CHUNK_WIDTH;
    if (newChunk != chunk) [[unlikely]] {
        chunk = newChunk;
        return true;
    }
    return false;
    x; // Need to have some kind of efficient per chunk entity list, just a vector is probably fine?
}
