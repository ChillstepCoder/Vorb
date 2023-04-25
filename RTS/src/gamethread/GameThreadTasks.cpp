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
    assert(sInstance); // TODO: had a crash here
    return *sInstance;
}

void GameThreadTasks::addCameraPickTeleportTask(const f32v3& camPos, const f32v3& camDir) {
    CameraPickTeleportData* teleportData = new CameraPickTeleportData{ camPos, camDir };
    mGameThreadProcs.enqueue(std::make_pair([](GameThread&, void* vData) {
        CameraPickTeleportData* data = static_cast<CameraPickTeleportData*>(vData);
        IEntityComponentSystem& ecs = sMainGameWorld->getECS();
        if (PhysicsComponent* phys = ecs.mRegistry.try_get<PhysicsComponent>(ecs.getLocalPlayer())) {
            PhysHitResult hitResult = sMainGameWorld->getPhysicsWorld().pick(data->camPos, data->camPos + data->camDir * 3000.0f, PICK_TYPE_ALL, PhysicsPickQueryFlags::QUERY_TILE_INFO);
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
        IEntityComponentSystem& ecs = sMainGameWorld->getECS();
        ecs.mRegistry.get<CharacterControlComponent>(ecs.getLocalPlayer()).mFlags.setBit(CharacterControlComponentFlags::HIDE_MODEL);
    }, (void*)hide));
}

void GameThreadTasks::addTileContainerStaticPhysicsMeshInitTask(TileContainerID containerId, StaticPhysicsMeshBuilder&& meshBuilder) {
    StaticPhysicsMeshBuilder* builderPtr = new StaticPhysicsMeshBuilder(std::move(meshBuilder));
    mGameThreadProcs.enqueue(std::make_pair([](GameThread&, void* vData) {
        StaticPhysicsMeshBuilder* builder = static_cast<StaticPhysicsMeshBuilder*>(vData);
        builder->finish(sMainGameWorld->getPhysicsWorld());
        // Release
        TileContainer* container = TileContainerRepository::getTileContainer(builder->getOwnerTileContainerID());
        //assert(container->getState() == TileContainerState::WAITING_MESH_AND_PHYSICS);
        container->setDidInitPhysics();
        container->decRef();
        delete builder;
    }, (void*)builderPtr));
}

void GameThreadTasks::addEntityCreateTask(const f32v3& pos, StrToken typeToken, bool shouldReplicate) {
    std::tuple<f32v3, StrToken, bool>* createData = new std::tuple<f32v3, StrToken, bool>(pos, typeToken, shouldReplicate);
    mGameThreadProcs.enqueue(std::make_pair([](GameThread&, void* vData) {
        std::tuple<f32v3, StrToken, bool>* createData = static_cast<std::tuple<f32v3, StrToken, bool>*>(vData);
        sMainGameWorld->getECS().createEntity(std::get<0>(*createData), std::get<1>(*createData), std::get<2>(*createData));
        delete createData;
    }, (void*)createData));
}
