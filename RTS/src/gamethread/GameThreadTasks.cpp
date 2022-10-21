#include "stdafx.h"
#include "GameThreadTasks.h"

#include "world/IWorld.h"
#include "ecs/IEntityComponentSystem.h"
#include "physics/PhysicsWorld.h"

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

void GameThreadTasks::addCameraPickInteractTask(const f32v3& camPos, const f32v3& camDir) {
    CameraPickTeleportData* teleportData = new CameraPickTeleportData{ camPos, camDir };
    mGameThreadProcs.enqueue(std::make_pair([](GameThread&, void* vData) {
        CameraPickTeleportData* data = static_cast<CameraPickTeleportData*>(vData);
        PhysHitResult hitResult = sWorld->getPhysicsWorld().pick(data->camPos, data->camPos + data->camDir * 3000.0f, PICK_TYPE_ALL);
        if (hitResult.didHit()) {
            // For interact must click in about the same spot
            //if (glm::length(mRightClickPickPos - hitResult.mPosition) < 0.05f) {
            //    f32v3 worldPos = hitResult.mPosition + hitResult.mNormal * 0.01f;
            //    WorldObjectQuery worldObjectQuery(worldPos);
            //    if (worldObjectQuery.isValid()) {
            //        // Right click picking
            //        mSelectedTileHandle = worldObjectQuery.getTileHandle();
            //        // Enable context menu
            //        mSelectedScreenPos = screenPos;
            //        mRightClickInteractPopup = std::make_unique<TileInteractPanel>(screenPos, static_cast<SDL_Window*>(m_app->getWindow().getHandle()), std::move(worldObjectQuery));
            //    }
            //}
        }
        delete data;
    }, teleportData));
}

