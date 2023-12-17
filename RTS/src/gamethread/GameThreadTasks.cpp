#include "stdafx.h"
#include "GameThreadTasks.h"

#include "gamethread/GameThread.h"

#include "world/World.h"
#include "ecs/IEntityComponentSystem.h"
#include "physics/PhysicsWorld.h"
#include "physics/StaticPhysicsMeshBuilder.h"

#include "options/DebugOptions.h"

GameThreadTasks* GameThreadTasks::sInstance = nullptr;

GameThreadTasks::GameThreadTasks(World& mainGameWorld) : mMainGameWorld(mainGameWorld) {

}

GameThreadTasks::~GameThreadTasks() {

}

GameThreadTasks& GameThreadTasks::initInstance(World& world) {
    if (!sInstance) {
        sInstance = new GameThreadTasks(world);
    }
    return *sInstance;
}

GameThreadTasks& GameThreadTasks::getInstance() {
    assert(sInstance); // TODO: had a crash here
    return *sInstance;
}

bool GameThreadTasks::exists() {
    return sInstance != nullptr;
}

void GameThreadTasks::addCameraPickTeleportTask(const f32v3& camPos, const f32v3& camDir) {

    struct CameraPickTeleportData {
        World* world;
        f32v3 camPos;
        f32v3 camDir;
    };
    CameraPickTeleportData* teleportData = new CameraPickTeleportData{ &mMainGameWorld, camPos, camDir };
    mGameThreadProcs.enqueue(std::make_pair([](GameThread&, void* vData) {
        CameraPickTeleportData* data = static_cast<CameraPickTeleportData*>(vData);
        IEntityComponentSystem& ecs = data->world->getECS();
        if (PhysicsComponent* phys = ecs.mRegistry.try_get<PhysicsComponent>(ecs.getLocalPlayer())) {
            PhysHitResult hitResult = data->world->getPhysicsWorld().pick(data->camPos, data->camPos + data->camDir * 3000.0f, PICK_TYPE_ALL, PhysicsPickQueryFlags::QUERY_TILE_INFO);
            if (hitResult.didHit()) {
                phys->teleportToPoint(hitResult.mPosition);
            }
        }
        delete data;
    }, teleportData));
}

void GameThreadTasks::addHideLocalPlayerModelTask(bool hide) {
    typedef std::pair<World*, bool> TaskData;
    TaskData* data = new TaskData{ &mMainGameWorld, hide };
    mGameThreadProcs.enqueue(std::make_pair([](GameThread&, void* vData) {
        TaskData* data = static_cast<TaskData*>(vData);
        IEntityComponentSystem& ecs = data->first->getECS();
        if (data->second) {
            ecs.mRegistry.get<CharacterControlComponent>(ecs.getLocalPlayer()).mFlags.setBit(CharacterControlComponentFlags::HIDE_MODEL);
        }
        else {
            ecs.mRegistry.get<CharacterControlComponent>(ecs.getLocalPlayer()).mFlags.clearBit(CharacterControlComponentFlags::HIDE_MODEL);
        }
        delete data;
    }, (void*)data));
}

void GameThreadTasks::addTileContainerStaticPhysicsMeshInitTask(const TileContainer* container, StaticPhysicsMeshBuilder&& meshBuilder) {
    typedef std::tuple<World*, const TileContainer*, StaticPhysicsMeshBuilder> TaskData;
    TaskData* taskData;
    taskData = new TaskData{ &container->getWorld(), container, std::move(meshBuilder) };
    mGameThreadProcs.enqueue(std::make_pair([](GameThread&, void* vData) {
        TaskData* taskData = static_cast<TaskData*>(vData);
        StaticPhysicsMeshBuilder& builder = std::get<2>(*taskData);
        World* world = std::get<0>(*taskData);
        builder.finish(world->getPhysicsWorld());
        // Release
        const TileContainer* container = std::get<1>(*taskData);
        //assert(container->getState() == TileContainerState::WAITING_MESH_AND_PHYSICS);
        container->setDidInitPhysics();
        container->decRef();
        delete taskData;
    }, (void*)taskData));
}

void GameThreadTasks::addEntityCreateTask(const f32v3& pos, StrToken typeToken, bool shouldReplicate) {
    typedef std::tuple<f32v3, StrToken, bool, World*> TaskData;
    TaskData* createData = new TaskData(pos, typeToken, shouldReplicate, &mMainGameWorld);
    mGameThreadProcs.enqueue(std::make_pair([](GameThread&, void* vData) {
        TaskData* createData = static_cast<TaskData*>(vData);
        std::get<3>(*createData)->getECS().createEntity(std::get<0>(*createData), std::get<1>(*createData), std::get<2>(*createData));
        delete createData;
    }, (void*)createData));
}
