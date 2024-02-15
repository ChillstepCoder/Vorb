#include "stdafx.h"
#include "GameThreadTasks.h"

#include "gamethread/GameThread.h"

#include "world/World.h"
#include "ecs/IEntityComponentSystem.h"
#include "physics/PhysicsWorld.h"
#include "physics/StaticPhysicsMeshBuilder.h"

#include "options/DebugOptions.h"

GameThreadTasks& GameThreadTasks::getInstance() {
    static GameThreadTasks sInstance;
    return sInstance;
}

void GameThreadTasks::updateMainThread() {
    constexpr ui32 BULK_DEQUEUE_SIZE = 16;
    // NOTE: Due to two separate queues, if one queue  is very full, then they may occur out of order!
    GameFunction procsCapture[BULK_DEQUEUE_SIZE];
    PreciseTimer timer;
    // TODO: Use optik for profiling

    if (const size_t count = GameThreadTasks::getInstance().mGameThreadFuncProcs.try_dequeue_bulk(procsCapture, BULK_DEQUEUE_SIZE)) {
        for (size_t i = 0; i < count; ++i) {
            procsCapture[i]();
        }
    }
    if (timer.stop() > 20.0f) {
        std::cout << timer.stop() << " ms *** GAME SPIKE WARNING ***\n";
    }
}

void GameThreadTasks::addCameraPickTeleportTask(World& world, const f32v3& camPos, const f32v3& camDir) {

    struct CameraPickTeleportData {
        World* world;
        f32v3 camPos;
        f32v3 camDir;
    };
    CameraPickTeleportData* teleportData = new CameraPickTeleportData{ &world, camPos, camDir };
    mGameThreadFuncProcs.enqueue([data = teleportData]() {
        IEntityComponentSystem& ecs = data->world->getECS();
        if (PhysicsComponent* phys = ecs.mRegistry.try_get<PhysicsComponent>(ecs.getLocalPlayer())) {
            PhysHitResult hitResult = data->world->getPhysicsWorld().pick(data->camPos, data->camPos + data->camDir * 3000.0f, PICK_TYPE_ALL, PhysicsPickQueryFlags::QUERY_TILE_INFO);
            if (hitResult.didHit()) {
                phys->teleportToPoint(hitResult.mPosition);
            }
        }
        delete data;
    });
}

void GameThreadTasks::addHideLocalPlayerModelTask(World& world, bool hide) {
    mGameThreadFuncProcs.enqueue([world = &world, hide]() {
        IEntityComponentSystem& ecs = world->getECS();
        if (hide) {
            ecs.mRegistry.get<CharacterControlComponent>(ecs.getLocalPlayer()).mFlags.setBit(CharacterControlComponentFlags::HIDE_MODEL);
        }
        else {
            ecs.mRegistry.get<CharacterControlComponent>(ecs.getLocalPlayer()).mFlags.clearBit(CharacterControlComponentFlags::HIDE_MODEL);
        }
    });
}

void GameThreadTasks::addTileContainerStaticPhysicsMeshInitTask(const TileContainer* container, StaticPhysicsMeshBuilder&& meshBuilder) {
    typedef std::tuple<World*, const TileContainer*, StaticPhysicsMeshBuilder> TaskData;
    TaskData* taskData;
    taskData = new TaskData{ &container->getWorld(), container, std::move(meshBuilder) };
    mGameThreadFuncProcs.enqueue([taskData]() {
        StaticPhysicsMeshBuilder& builder = std::get<2>(*taskData);
        World* world = std::get<0>(*taskData);
        builder.finish(world->getPhysicsWorld());
        // Release
        const TileContainer* container = std::get<1>(*taskData);
        //assert(container->getState() == TileContainerState::WAITING_MESH_AND_PHYSICS);
        container->setDidInitPhysics();
        container->decRef();
        delete taskData;
    });
}

void GameThreadTasks::addEntityCreateTask(World& world, const f32v3& pos, StrToken typeToken, bool shouldReplicate) {
    mGameThreadFuncProcs.enqueue([world = &world, pos, typeToken, shouldReplicate]() {
        world->getECS().createEntity(pos, typeToken, shouldReplicate);
    });
}
