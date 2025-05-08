#pragma once

enum class ChunkEventType {
	GrassEdit
};

struct ChunkEvent {
	LocalChunk& chunk;
	ChunkEventType type;
	TileIndex tileIndex;
};

EVENT_DISPATCHER_TYPE(LocalChunk, ChunkEventType, const ChunkEvent&);