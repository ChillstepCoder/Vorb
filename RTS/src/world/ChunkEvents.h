#pragma once

enum class ChunkEventType {
	GrassEdit
};

struct ChunkEvent {
	Chunk& chunk;
	ChunkEventType type;
	TileIndex tileIndex;
};

EVENT_DISPATCHER_TYPE(Chunk, ChunkEventType, const ChunkEvent&);