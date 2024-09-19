#pragma once

class LocalChunk;
class Building;

enum class TileContainerState : ui8 {
    LOADING,
    READY
};

enum class TileContainerOwnerType : ui8 {
    CHUNK,
    BUILDING,
    COUNT
};
typedef std::variant<LocalChunk*, Building*> VarTileContainerOwner;