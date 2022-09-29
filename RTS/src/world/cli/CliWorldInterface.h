#pragma once

class CloudManager;
class Chunk;
class Camera3D;

class CliWorldInterface
{
public:
    CliWorldInterface();
    virtual ~CliWorldInterface();

    void tickClient();
    // Pure virtual interface
    virtual void onFrameBegin() = 0;
    virtual void frameUpdate(const Camera3D& camera, f32 elapsedSec) = 0;

    size_t getNumVisibleChunks() const { return mVisibleChunks.size(); }
    void enumVisibleChunks(std::function<void(const Chunk&)> func) const;

    // Accessors
    const CloudManager& getCloudManager() const { return *mCloudManager; }

protected:
    void updateChunkVisibility(const Camera3D& camera, const std::vector<Chunk*>& activeChunks);
    void updateParticleSystems(const f32v2& playerPos);
    void updateClouds();
    void initPostResourcesLoadedClient();

    // Clouds
    std::unique_ptr<CloudManager> mCloudManager;

    // Sunlight
    float mSunHeight = 1.0f;
    f32v3 mSunPosition = f32v3(0.0f, 0.0f, 1.0f);
    float mTimeOfDay = 0.0f; // span of 24:00
    f32v3 mSunColor = f32v3(1.0f);
    f32m4 mSkyRotMatrix = f32m4(1.0f);

    // Client rendering
    std::vector<Chunk*> mVisibleChunks;
};

