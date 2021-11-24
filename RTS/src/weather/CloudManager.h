#pragma once


class World;
class TBOBillboardMesh;


struct Cloud {
    f32v3 pos;
    f32 size;
};

class CloudManager
{
public:
    friend class CloudRenderer;
    CloudManager(const World& world);
    ~CloudManager();

    void update();

private:
    void addCloudAt(const f32v3& pos, f32 size);

    mutable std::unique_ptr<TBOBillboardMesh> mCloudMesh;
    std::vector<Cloud> mClouds;
    const World& mWorld;
};

