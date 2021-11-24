#include "stdafx.h"
#include "CloudManager.h"

#include "rendering/QuadMesh.h"

#include "World.h"

#include "Random.h"

#include "generation/WorldGenerationData.h"

CloudManager::CloudManager(const World& world) : mWorld(world)
{
    
}

CloudManager::~CloudManager()
{

}

void CloudManager::update() {

    // TODO: Async load
    const f32v2& loadCenter = mWorld.getLoadCenter();
    f32v3 pos(loadCenter.x, loadCenter.y, 16.0f);
    static bool hasinit = false;
    mClouds.reserve(10000);
    if (!hasinit) {
        PreciseTimer timer;
        hasinit = true;
        for (int y = -1000; y < 1000; y += 2) {
            for (int x = -1000; x < 1000; x += 2) {
                if (Random::getCachedRandomfSpecific(x * 2232 + y * 14302) >= 0.4f) {
                    const float n = sWorldGenData.mCloudsNoise.compute(x, y);
                    if (n > 0.3f) {
                        float size = (n - 0.3f) * 6.0f + 25.0f;
                        float heightOffset = Random::getCachedRandomfSpecific(x * 14102 + y * 2315) * 3.0f + (n - 0.3f) * 3.0f + 25.0f;
                        if (n > 0.6f) {
                            size += 30.0f;
                            heightOffset += 10.0f;
                        }
                        addCloudAt(pos + f32v3(x, y, heightOffset), size);
                    }
                }
            }
        }
        std::cout << "Generated all clouds in " << timer.stop() << " ms\n";
    }
}

void CloudManager::addCloudAt(const f32v3& pos, f32 size) {
    mClouds.emplace_back(Cloud{ pos, size });
}
