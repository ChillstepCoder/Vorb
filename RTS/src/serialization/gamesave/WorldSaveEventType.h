#pragma once

#include "filesystem/FileSystem.h"

enum class WorldSaveEventType {
    SaveBegin,
    SaveEnd
};

struct WorldSaveEvent {
    fs::path savePath;
};
EVENT_DISPATCHER_TYPE(WorldSave, WorldSaveEventType, const WorldSaveEvent&);
