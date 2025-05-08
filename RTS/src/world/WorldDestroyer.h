#pragma once

class World;

class WorldDestroyer
{
public:
    // Caller should free memory after
    static void shutdownWorld(World& world);
    static void shutdownAllWorlds();

    static World* gameThreadUpdate();

    inline static std::mutex mShutdownWorldMutex;
    inline static World* mCurrentlyShuttingDownWorld = nullptr;
};

