#pragma once

class Chunk;
class Building;

enum class TileContainerState : ui8 {
    LOADING,
    WAITING_MESH_AND_PHYSICS,
    READY
};

enum class TileContainerOwnerType : ui8 {
    CHUNK,
    BUILDING,
    COUNT
};
typedef std::variant<Chunk*, Building*> VarTileContainerOwner;