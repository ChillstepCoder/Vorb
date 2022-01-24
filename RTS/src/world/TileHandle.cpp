#include "stdafx.h"
#include "TileHandle.h"

#include "Chunk.h"


TileRef::TileRef(Chunk* chunk, TileIndex index) :
    chunk(chunk),
    index(index),
    tile(&chunk->mTiles[index]) {
    chunk->incRef();
}

TileRef::TileRef(TileHandle handle) :
    chunk(const_cast<Chunk*>(handle.chunk)), // FUCK YOU I DO WHAT I WANT
    index(handle.index),
    tile(&chunk->mTiles[handle.index]) {
    chunk->incRef();
}

TileRef::TileRef() {

}

void TileRef::acquire(TileHandle handle) {
    assert(IS_MAIN_THREAD());
    assert(!chunk);
    chunk = const_cast<Chunk*>(handle.chunk); // FUCK YOU I DO WHAT I WANT;
    index = handle.index;
    tile = &chunk->mTiles[index];
    chunk->incRef();
}

void TileRef::acquire(Chunk* newChunk, TileIndex newIndex) {
    assert(IS_MAIN_THREAD());
    assert(!chunk);
    chunk = newChunk;
    index = newIndex;
    tile = &chunk->mTiles[newIndex];
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
    tile(&chunk->mTiles[index]) {

}

f32v2 TileHandle::getWorldPos() {
    return chunk->getWorldPos() + f32v2(index.getX(), index.getY());
}
