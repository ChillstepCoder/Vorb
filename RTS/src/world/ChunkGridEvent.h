#pragma once

class LocalChunk;
class ChunkGrid;

class ChunkGridEvent {
public:
    ChunkGridEvent(LocalChunk& chunk) : chunk(chunk) {}

    LocalChunk& chunk;
};

enum class CHUNK_GRID_EVENT_TYPE {
    BeginActivate,
    BeginLoad,
    Activated,
    Deactivated
};
EVENT_DISPATCHER_TYPE(ChunkGrid, CHUNK_GRID_EVENT_TYPE, ChunkGridEvent&);

