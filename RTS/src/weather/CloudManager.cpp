#include "stdafx.h"
#include "CloudManager.h"

#include "rendering/QuadMesh.h"

#include "World.h"

#include "Random.h"


CloudManager::CloudManager(const World& world) : mWorld(world)
{
    
}

CloudManager::~CloudManager()
{

}

void CloudManager::update() {

    const f32v2& loadCenter = mWorld.getLoadCenter();
    f32v3 pos(loadCenter.x, loadCenter.y, 16.0f);
    static bool hasinit = false;
    if (!hasinit) {
        hasinit = true;
        for (int y = -100; y < 100; y += 5) {
            for (int x = -100; x < 100; x += 5) {
                addCloudAt(pos + f32v3(x + Random::getCachedRandomfSpecific(x * 123 + y * 43) * 2.0, y + Random::getCachedRandomfSpecific(x * 1233 + y * 443) * 2.0, Random::getCachedRandomfSpecific(x * 13 + y * 3) * 16.0f), Random::getCachedRandomfSpecific(x + y * 25) * 4.0f + 10.0f);
            }
        }
    }
}

void CloudManager::addCloudAt(const f32v3& pos, f32 size) {
    mClouds.emplace_back(Cloud{ pos, size });
}
