#include "stdafx.h"
#include "GameThreadTasks.h"

#include "world/IWorld.h"
#include "ecs/IEntityComponentSystem.h"
#include "physics/PhysicsWorld.h"
#include "physics/StaticPhysicsMeshBuilder.h"

#include "options/DebugOptions.h"

struct CameraPickTeleportData {
    f32v3 camPos;
    f32v3 camDir;
};

GameThreadTasks* GameThreadTasks::sInstance = nullptr;

GameThreadTasks::GameThreadTasks() {

}

GameThreadTasks::~GameThreadTasks() {

}

GameThreadTasks& GameThreadTasks::initInstance() {
    if (!sInstance) {
        sInstance = new GameThreadTasks();
    }
    return *sInstance;
}

GameThreadTasks& GameThreadTasks::getInstance() {
    assert(sInstance);
    return *sInstance;
}

void GameThreadTasks::addCameraPickTeleportTask(const f32v3& camPos, const f32v3& camDir) {
    CameraPickTeleportData* teleportData = new CameraPickTeleportData{ camPos, camDir };
    mGameThreadProcs.enqueue(std::make_pair([](GameThread&, void* vData) {
        CameraPickTeleportData* data = static_cast<CameraPickTeleportData*>(vData);
        IEntityComponentSystem& ecs = sWorld->getECS();
        if (PhysicsComponent* phys = ecs.mRegistry.try_get<PhysicsComponent>(ecs.getLocalPlayer())) {
            PhysHitResult hitResult = sWorld->getPhysicsWorld().pick(data->camPos, data->camPos + data->camDir * 3000.0f, PICK_TYPE_ALL);
            if (hitResult.didHit()) {
                phys->teleportToPoint(hitResult.mPosition);
            }
        }
        delete data;
    }, teleportData));
}

void GameThreadTasks::addHideLocalPlayerModelTask(bool hide) {
    mGameThreadProcs.enqueue(std::make_pair([](GameThread&, void* vData) {
        bool hidePlayerModel = (bool)vData;
        IEntityComponentSystem& ecs = sWorld->getECS();
        ecs.mRegistry.get<CharacterControlComponent>(ecs.getLocalPlayer()).mHideModel = hidePlayerModel;
    }, (void*)hide));
}

void GameThreadTasks::addTileContainerStaticPhysicsMeshInitTask(TileContainerID containerId, StaticPhysicsMeshBuilder&& meshBuilder) {
    StaticPhysicsMeshBuilder* builderPtr = new StaticPhysicsMeshBuilder(std::move(meshBuilder));
    mGameThreadProcs.enqueue(std::make_pair([](GameThread&, void* vData) {
        StaticPhysicsMeshBuilder* builder = static_cast<StaticPhysicsMeshBuilder*>(vData);
        builder->finish(sWorld->getPhysicsWorld());
    }, (void*)builderPtr));
}
