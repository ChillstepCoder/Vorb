#include "stdafx.h"
#include "TileHandle.h"

#include "Chunk.h"


TileRef::TileRef(Chunk* chunk, TileIndex index) :
    chunk(chunk),
    index(index),
    tile(chunk->mTiles[index]) {
    chunk->incRef();
}

TileRef::TileRef(TileHandle handle) :
    chunk(const_cast<Chunk*>(handle.chunk)), // FUCK YOU I DO WHAT I WANT
    index(handle.index),
    tile(chunk->mTiles[handle.index]) {
    chunk->incRef();
}

void TileRef::release()
{
    if (chunk) {
        chunk->decRef();
        chunk = nullptr;
    }
}

TileHandle::TileHandle(const Chunk* chunk, TileIndex index) :
    chunk(chunk),
    index(index),
    tile(chunk->mTiles[index]) {

}

f32v2 TileHandle::getWorldPos() {
    return chunk->getWorldPos() + f32v2(index.getX(), index.getY());
}
