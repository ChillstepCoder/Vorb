#pragma once

class Chunk;
class ChunkGrid;

class ChunkGridEvent {
public:
    ChunkGridEvent(Chunk& chunk) : chunk(chunk) {}

    Chunk& chunk;
};

enum class CHUNK_GRID_EVENT_TYPE {
    BeginActivate,
    Ready,
    Deactivate
};
EVENT_DISPATCHER_TYPE(ChunkGrid, CHUNK_GRID_EVENT_TYPE, ChunkGridEvent&);

