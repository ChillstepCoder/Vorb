#include "stdafx.h"
#include "GameThreadTasks.h"

#include "gamethread/GameThread.h"

#include "tile/TileContainer.h"
#include "world/World.h"
#include "ecs/IFullECS.h"
#include "physics/StaticPhysicsMeshBuilder.h"

#include "physics/PhysicsWorld.h"
#include "physics/PhysHitResult.h"
#include "physics/PhysicsBroadPhaseLayerFilters.h"

#include "options/DebugOptions.h"


GameThreadTasks::GameThreadTasks() : mToken(mGameThreadFuncProcs) {

}

GameThreadTasks& GameThreadTasks::getInstance() {
    static GameThreadTasks sInstance;
    return sInstance;
}

void GameThreadTasks::updateMainThread() {
    constexpr ui32 BULK_DEQUEUE_SIZE = 8;
    GameFunction procs[BULK_DEQUEUE_SIZE];
    PreciseTimer timer;
    // TODO: Use optik for profiling
    constexpr f32 BUDGET_MS = 3.0f;
    do {
        if (const size_t count = mGameThreadFuncProcs.try_dequeue_bulk(mToken, procs, BULK_DEQUEUE_SIZE)) {
            for (size_t i = 0; i < count; ++i) {
                procs[i]();
            }
        }
    } while (timer.stop() < BUDGET_MS);
    if (timer.stop() > 20.0f) {
        LOG_WARN("{} ms *** GAME SPIKE WARNING ***", timer.stop());
    }
}

void GameThreadTasks::addCameraPickTeleportTask(World& world, f32v3 camPos, f32v3 camDir) {

    struct CameraPickTeleportData {
        World* world;
        f32v3 camPos;
        f32v3 camDir;
    };
    CameraPickTeleportData* teleportData = new CameraPickTeleportData{ &world, camPos, camDir };
    mGameThreadFuncProcs.enqueue([data = teleportData]() {
        IFullECS& ecs = data->world->getECS();
        if (PhysicsComponent* phys = ecs.mRegistry.try_get<PhysicsComponent>(ecs.getLocalPlayer())) {
            PhysHitResult hitResult = data->world->getPhysicsWorld().raycastFirst(data->camPos, data->camPos + data->camDir * 3000.0f, PhysicsBroadphaseLayerFilterStatic(), {}, {}, true /*traceFarTerrain*/);
            if (hitResult.didHit()) {
                phys->teleportBottomToPoint(hitResult.mPosition);
            }
        }
        delete data;
    });
}

void GameThreadTasks::addHideLocalPlayerModelTask(World& world, bool hide) {
    mGameThreadFuncProcs.enqueue([world = &world, hide]() {
        IFullECS& ecs = world->getECS();
        if (hide) {
            ecs.mRegistry.get<CharacterControlComponent>(ecs.getLocalPlayer()).mFlags.setBit(CharacterControlComponentFlags::HideModel);
        }
        else {
            ecs.mRegistry.get<CharacterControlComponent>(ecs.getLocalPlayer()).mFlags.clearBit(CharacterControlComponentFlags::HideModel);
        }
    });
}

void GameThreadTasks::addTileContainerStaticPhysicsMeshUpdateTask(const TileContainer* container, StaticPhysicsMeshBuilder&& meshBuilder) {
    typedef std::tuple<World*, const TileContainer*, StaticPhysicsMeshBuilder> TaskData;
    // TODO: Figure out how to not have new here (deleted copy constructor issue?)
    StaticPhysicsMeshBuilder* builderData = new StaticPhysicsMeshBuilder(std::move(meshBuilder));
    mGameThreadFuncProcs.enqueue([world = &container->getWorld(), container, builderData]() {
        builderData->finish(world->getPhysicsWorld());
        //container->setDidInitPhysics();
        container->decRef();
        delete builderData;
    });
}

void GameThreadTasks::addEntityCreateTask(World& world, const f32v3& pos, StrToken typeToken, bool shouldReplicate) {
    mGameThreadFuncProcs.enqueue([world = &world, pos, typeToken, shouldReplicate]() {
        world->getECS().createEntity(pos, typeToken, shouldReplicate);
    });
}
