#include "stdafx.h"
#include "EditorWorldInterfaceController.h"

#include "camera/CameraController.h"
#include "gamethread/GameThreadTasks.h"
#include "resources/ResourceManager.h"
#include "item/ItemRepository.h"
#include "item/ItemStockpile.h"
#include "item/ItemReservation.h"
#include "item/ItemStockpileRegistry.h"
#include "rendering/RenderContext.h"
#include "world/World.h"
#include "physics/PhysicsWorld.h"
#include "ecs/IEntityComponentSystem.h"
#include "ecs/factory/EntityFactory.h"
#include "ui/UIContext.h"
#include "pathfinding/NavWorld.h"

#include "resources/TileRepository.h"

#include <imgui.h>
#include <SDL.h>
#include <Vorb/ui/GameWindow.h>

#include "options/DebugOptions.h"

void EditorWorldInterfaceController::init()
{
    assert(mWorld);
    initEvents();
}

EditorWorldInterfaceController::~EditorWorldInterfaceController() {

}

void EditorWorldInterfaceController::update()
{
    ASSERT_RENDER_THREAD();
    assert(mWorld);

    if (vui::InputDispatcher::key.isKeyPressed(VKEY_N)) {
        // TODO: ITEMFactory

        GameThreadTasks::getInstance().addGenericTask([](GameThread&, void* vWorld) {
            World* world = static_cast<World*>(vWorld);
            ItemID id = ItemRepository::get().getAssetID(CStrToken("wood_raw"));
            ItemStack newStack(id, 1);
            EntityFactory::createItemProjectile(*world, world->getECS().getLocalPlayerPosition() + f32v3(0.0f, 0.0f, 1.0f), f32v3(0.0f), newStack);
            sDebugOptions.mCities = !sDebugOptions.mCities;
        }, (void*)mWorld);
    }

    if (mIsQuerying && mWorldObjectQuery && mWorldObjectQuery->isValid()) {
        // Right click picking
        mSelectedTileHandle = mWorldObjectQuery->getTileHandle();
        // Enable context menu
        mRightClickInteractPopup = std::make_unique<TileInteractPanel>(*mWorld, mSelectedScreenPos, static_cast<SDL_Window*>(mWindow->getHandle()), mWorldObjectQuery);
        mIsQuerying = false;
    }

    updateTilePicking();

    UIContext::getInstance().updateEditors(mWorld, mCameraController->getOwnedCamera(), mMousePickRay);
}

void EditorWorldInterfaceController::renderUI() {
    tryUpdateAndRenderInteractPopup();
}
//  TODO: REMOVE
#include "effect/IEffectContext.h"
#include "rendering/particle/ParticleSystemInputs.h"
#include "debugging/DebugRenderer.h"
void EditorWorldInterfaceController::updateTilePicking() {
    const f32 normalizedX = (mMousePosition.x / (f32)mWindow->getWidth()) * 2.0f - 1.0f;
    const f32 normalizedY = -((mMousePosition.y / (f32)mWindow->getHeight()) * 2.0f - 1.0f);
    f32v4 pickRayClipSpace(normalizedX, normalizedY, -1.0f, 1.0f);
    f32v4 pickRayEyeSpace = glm::inverse(mCameraController->getOwnedCamera().getProjectionMatrix()) * pickRayClipSpace;
    pickRayEyeSpace.z = -1.0f;
    pickRayEyeSpace.w = 0.0f;
    f32v4 pickRayWorldSpace = glm::inverse(mCameraController->getOwnedCamera().getViewMatrix()) * pickRayEyeSpace;
    f32v3 pickRayXYZ(pickRayWorldSpace.x, pickRayWorldSpace.y, pickRayWorldSpace.z);
    mMousePickRay = glm::normalize(pickRayXYZ);

    if (mRightClickDownPick && mRightClickDownPick->isDone()) {
        PhysHitResult hitResult = mRightClickDownPick->getLastPickResult();
        if (hitResult.didHit()) {
            mRightClickPickPos = hitResult.mPosition;
        }
        else {
            mRightClickPickPos = f32v3(FLT_MAX);
        }
        mRightClickDownPick.reset();
    }
    if (mRightClickUpPick && mRightClickUpPick->isDone()) {
        PhysHitResult hitResult = mRightClickUpPick->getLastPickResult();
        if (hitResult.didHit()) {

            // TMP REMOVE
            if (vui::InputDispatcher::key.isKeyPressed(VKEY_I)) {
                DebugRenderer::drawWireQuad(hitResult.mPosition + f32v3(0.0f, 0.0f, 0.5f), f32v2(0.3f), color4(1.0f, 0.0f, 0.0f, 1.0f), 100);
                ParticleSystemInputs inputs;
                mWorld->getEffectContext().playParticleEffectAtPoint(CStrToken("hitfx"), hitResult.mPosition + f32v3(0.0f, 0.0f, 0.5f), inputs, BitFlags<EffectCreateFlags>());
                mRightClickUpPick.reset();
                return;
            }

            mSelectedScreenPos = mRightClickUpPickScreenPos;
            // For interact must click in about the same spot
            if (hitResult.mSelectedEntity != INVALID_ENTITY) {
                LOG_CRITICAL("Selected Entity");
            }
            else {
                if (glm::length(mRightClickPickPos - hitResult.mPosition) < 0.05f) {
                    TileContainerID containerOwner = hitResult.mContainerID;
                    if (containerOwner != INVALID_TILE_CONTAINER_ID) {
                        TileIndex index = hitResult.mTileIndex;
                        // ONLY WORKS FOR MODELS
                        if (index != INVALID_TILE_INDEX) {
                            // Query whatever we selected
                            mWorldObjectQuery = WorldObjectQueryFactory::makeQuery(*mWorld, LiteTileHandle(containerOwner, index));
                            mIsQuerying = true;
                        }
                    }
                    else {
                        // Selected terrain
                        f32v3 worldPos = hitResult.mPosition + hitResult.mNormal * 0.01f;
                        mWorldObjectQuery = WorldObjectQueryFactory::makeQuery(*mWorld, worldPos);
                        mIsQuerying = true;
                    }
                }
            }
        }
        mRightClickUpPick.reset();
    }
}


void EditorWorldInterfaceController::initEvents() {
    vui::InputDispatcher::key.registerKeyListeners(mKeyListeners);
    vui::InputDispatcher::mouse.registerMouseListeners(mMouseListeners);

    vui::InputDispatcher::key.addKeyDownListener(mKeyListeners, [this](const vui::KeyEvent& event) {
        // Wait for initialization to prevent crash from input
        // TODO: Move this higher? We could still crash in another listener...
        if (!GameThreadTasks::exists()) {
            return;
        }

        // View toggle
        if (event.keyCode == VKEY_B && event.mod.lShift) {
            sDebugOptions.mWireframe = !sDebugOptions.mWireframe;
        }
        else if (event.keyCode == VKEY_C && event.mod.lShift) {
            sDebugOptions.mChunkBoundaries = !sDebugOptions.mChunkBoundaries;
        }
        else if (event.keyCode == VKEY_V && event.mod.lShift) {
            sDebugOptions.mCities = !sDebugOptions.mCities;
        }
        else if (event.keyCode == VKEY_J && event.mod.lShift) {
            sDebugOptions.mShowNavGraphUpdates = !sDebugOptions.mShowNavGraphUpdates;
        }
        else if (event.keyCode == VKEY_R && event.mod.lCtrl) {
            Services::ResourceManager::ref().reloadMaterials();
        }
        else if (event.keyCode == VKEY_N && event.mod.lShift) {
            RenderContext::getInstance().selectNextDebugShader();
        }
        else if (event.keyCode == VKEY_U) {
            if (sDebugOptions.mCameraMode == CameraMode::MMO) {
                sDebugOptions.mCameraMode = CameraMode::FIRST_PERSON;
                GameThreadTasks::getInstance().addHideLocalPlayerModelTask(true);
            }
            else {
                sDebugOptions.mCameraMode = CameraMode::MMO;
                GameThreadTasks::getInstance().addHideLocalPlayerModelTask(false);
            }
        }
        else if (event.keyCode == VKEY_F && event.mod.lShift) {
            if (sDebugOptions.mCameraMode == CameraMode::FREE_LOOK) {
                sDebugOptions.mCameraMode = CameraMode::MMO;
            }
            else {
                sDebugOptions.mCameraMode = CameraMode::FREE_LOOK;
            }
        }
        else if (event.keyCode == VKEY_P && event.mod.lShift) {
            GameThreadTasks::getInstance().addGenericTask([](GameThread&, void* vWorld) {
                World* world = static_cast<World*>(vWorld);
                if (world->getPhysicsWorld().isProfiling()) {
                    world->getPhysicsWorld().endB3ProfilingAndDumpToFile("bullet_timings");
                }
                else {
                    world->getPhysicsWorld().startB3Profiling();
                }
            }, (void*)mWorld);
        }
        else if (event.keyCode == VKEY_ESCAPE) {
            UIContext::getInstance().toggleMainMenu();
        }
        
    });

    vui::InputDispatcher::mouse.addButtonDownListener(mMouseListeners, [this](const vui::MouseButtonEvent& event) {
        // Fix this
        UIContext::getInstance().closeTileInspectionPanel();
        if (event.button == vorb::ui::MouseButton::RIGHT) {
            // If we are awaiting a pick result, dont send another
            if (mRightClickDownPick || mRightClickUpPick) {
                return;
            }
            if (mCameraController) {
                mRightClickDownPick = std::make_unique<DeferredPhysicsPick>();
                const f32v3 camPos = mCameraController->getOwnedCamera().getPosition();
                mWorld->getPhysicsWorld().pickDeferred(mRightClickDownPick.get(), camPos, camPos + mMousePickRay * 3000.0f, PICK_TYPE_ALL, PhysicsPickQueryFlags::QUERY_TILE_INFO);
                mRightClickTimer.start();

            }
        }
    });

    vui::InputDispatcher::mouse.addMotionListener(mMouseListeners, [this](const vui::MouseMotionEvent& event) {
        mMousePosition.x = (f32)event.x;
        mMousePosition.y = (f32)event.y;
    });

    vui::InputDispatcher::mouse.addButtonUpListener(mMouseListeners, [this](const vui::MouseButtonEvent& event) {
        constexpr float VEL_MULT = 0.0001f;
        constexpr float VEL_EXP = 0.4f;
        const f32v2 screenPos(event.x, event.y);

        if (event.button == vui::MouseButton::LEFT) {
            if (vui::InputDispatcher::key.isKeyPressed(VKEY_T)) {
                // Teleport
                GameThreadTasks::getInstance().addCameraPickTeleportTask(mCameraController->getOwnedCamera().getPosition(), mMousePickRay);
            }
            else {
                if (mRightClickInteractPopup) {
                    mRightClickInteractPopup.reset();
                }
            }
        }
        else if (event.button == vui::MouseButton::RIGHT) {
            constexpr f64 RIGHT_CLICK_INTERACT_MS_THRESHOLD = 220.0;
            if (mRightClickInteractPopup) {
                mRightClickInteractPopup.reset();
            }
            else if (!mRightClickUpPick && mRightClickTimer.stop() < RIGHT_CLICK_INTERACT_MS_THRESHOLD) {
                const f32v3& camPos = mCameraController->getOwnedCamera().getPosition();
                mRightClickUpPick = std::make_unique<DeferredPhysicsPick>();
                mWorld->getPhysicsWorld().pickDeferred(mRightClickUpPick.get(), camPos, camPos + mMousePickRay * 3000.0f, PICK_TYPE_ALL, PhysicsPickQueryFlags::QUERY_TILE_INFO);
                mRightClickUpPickScreenPos = screenPos;
            }
        }

    });
}

void EditorWorldInterfaceController::tryUpdateAndRenderInteractPopup() {
    // Handle interact menu TODO: Notify to get this out of here
    if (mRightClickInteractPopup) {
        // Render selected
        const f32v3 worldPos = mSelectedTileHandle.getWorldPos3D();
        const i32v2 worldPosInt = worldPos;

        const UIInteractMenuResultFlags result = mRightClickInteractPopup->updateAndRender();
        // TODO: Notify
        if (result & INTERACT_MENU_RESULT_PATHFIND) {
            typedef std::pair<World*, TileHandle> TaskData;
            TaskData* taskData = new TaskData(mWorld, mSelectedTileHandle);
            if (mSelectedTileHandle.isValid()) {
                GameThreadTasks::getInstance().addGenericTask([](GameThread&, void* vTaskData) {
                    // TODO: Small race condition here if tile handle changes or chunk is destroyed
                    TaskData* data = static_cast<TaskData*>(vTaskData);
                    const TileHandle& tileHandle = data->second;
                    if (tileHandle.isValid()) {
                        IEntityComponentSystem& ecs = data->first->getECS();
                        PhysicsComponent& physCmp = ecs.mRegistry.get<PhysicsComponent>(ecs.getLocalPlayer());
                        NavigationComponent& cmp = ecs.mRegistry.get_or_emplace<NavigationComponent>(ecs.getLocalPlayer());
                        cmp.requestCoarsePath(physCmp.getPosition(), tileHandle.getWorldPos3D(), nullptr);
                    }
                    delete data;
                }, (void*)taskData);
            }
        }
        else if (result & INTERACT_MENU_RESULT_CLEAR_TILE) {
            if (mSelectedTileHandle.isValid()) {
                TileHandle* tileHandlePtr = new TileHandle(mSelectedTileHandle);
                GameThreadTasks::getInstance().addGenericTask([](GameThread&, void* vTileHandlePtr) {
                    TileHandle* tileHandlePtr = static_cast<TileHandle*>(vTileHandlePtr);
                    TileContainer* container = tileHandlePtr->getMutableContainer();
                    container->setTileLayer(tileHandlePtr->tileIndex, TileLayer::Ground, TILE_ID_NONE);
                    container->setTileLayer(tileHandlePtr->tileIndex, TileLayer::Main, TILE_ID_NONE);
                    delete tileHandlePtr;
                }, tileHandlePtr);
            }
        }
        else if (result & INTERACT_MENU_RESULT_PLANT_TREE) {
            if (mSelectedTileHandle.isValid()) {
                TileHandle* tileHandlePtr = new TileHandle(mSelectedTileHandle);
                GameThreadTasks::getInstance().addGenericTask([](GameThread&, void* vTileHandlePtr) {
                    TileHandle* tileHandlePtr = static_cast<TileHandle*>(vTileHandlePtr);
                    tileHandlePtr->getMutableContainer()->setTileLayer(tileHandlePtr->tileIndex, TileLayer::Main, TileRepository::get().getTileID(CStrToken("tree_a")));
                    delete tileHandlePtr;
                }, tileHandlePtr);
            }
        }
        else if (result & INTERACT_MENU_RESULT_PLANT_TREE_2) {
            if (mSelectedTileHandle.isValid()) {
                TileHandle* tileHandlePtr = new TileHandle(mSelectedTileHandle);
                GameThreadTasks::getInstance().addGenericTask([](GameThread&, void* vTileHandlePtr) {
                    TileHandle* tileHandlePtr = static_cast<TileHandle*>(vTileHandlePtr);
                    tileHandlePtr->getMutableContainer()->setTileLayer(tileHandlePtr->tileIndex, TileLayer::Main, TileRepository::get().getTileID(CStrToken("bush_med")));
                    delete tileHandlePtr;
                }, tileHandlePtr);
            }
        }
        else if (result & INTERACT_MENU_RESULT_BUILD_WALL) {
            if (mSelectedTileHandle.isValid()) {
                TileHandle* tileHandlePtr = new TileHandle(mSelectedTileHandle);
                GameThreadTasks::getInstance().addGenericTask([](GameThread&, void* vTileHandlePtr) {
                    TileHandle* tileHandlePtr = static_cast<TileHandle*>(vTileHandlePtr);
                    tileHandlePtr->getMutableContainer()->setTileLayer(tileHandlePtr->tileIndex, TileLayer::Main, TileRepository::get().getTileID(CStrToken("rock1")));
                    delete tileHandlePtr;
                }, tileHandlePtr);
            }
        }
        else if (result & INTERACT_MENU_RESULT_INSPECT) {
            // grass
            UIContext::getInstance().activateTileInspectionPanel(mSelectedScreenPos, mSelectedTileHandle);
        }
        else if (result & INTERACT_MENU_RESULT_DEBUG_ADD_25_WOOD) {
            // grass
            WorldObjectQueryPtr& worldObjects = mRightClickInteractPopup->getWorldObjects();
            ItemStockpile* stockPile = worldObjects->getStockpile();
            assert(stockPile);
            ItemStack woodPile;
            // TODO: AssetHandle?
            woodPile.id = ItemRepository::get().getAssetID(CStrToken("wood_raw"));
            woodPile.quantity = 25;
            if (std::unique_ptr<ItemReservation> itemPromise = stockPile->tryPromiseItemStack(woodPile, 1)) {
                while (!itemPromise->isFinished()) {
                    itemPromise->fulfillCurrentTarget(woodPile);
                }
            }
        }
        else if (result & INTERACT_MENU_RESULT_DEBUG_DESTROY_STOCK) {
            // grass
            WorldObjectQueryPtr& worldObjects = mRightClickInteractPopup->getWorldObjects();
            ItemStockpile* stockPile = worldObjects->getStockpile();
            mWorld->getItemStockpileRegistry().destroyStockpile(stockPile);
        }
        else if (result & INTERACT_MENU_RESULT_DEBUG_KILL_AGENT) {
            assert(false);
        }
        else if (result & INTERACT_MENU_RESULT_DEBUG_NAVMESH) {
            const WorldNetMode netMode = mWorld->getNetMode();
            if (netMode == WorldNetMode::Host) {
                TileHandle tileHandle = mRightClickInteractPopup->getSelectedTileHandle();
                mWorld->tryGetNavWorld()->debugDrawCoarseNavGraphForContainer(*tileHandle.container, nullptr, 2000);
            }
            else {
                assert(false);
            }
        }
        else if (result & INTERACT_MENU_RESULT_DEBUG_FINE_NAVMESH) {
            if (mWorld->getNetMode() == WorldNetMode::Host) {
                TileHandle tileHandle = mRightClickInteractPopup->getSelectedTileHandle();
                mWorld->tryGetNavWorld()->debugDrawFineNavGraphForContainer(*tileHandle.container, 2000);
            }
            else {
                assert(false);
            }
        }
        else if (result & INTERACT_MENU_RESULT_DEBUG_NAV_NODE) {
            if (mWorld->getNetMode() == WorldNetMode::Host) {
                TileHandle tileHandle = mRightClickInteractPopup->getSelectedTileHandle();
                mWorld->tryGetNavWorld()->debugDrawCoarseNavNode(tileHandle, nullptr, 2000);
            }
            else {
                assert(false);
            }
        }
        else if (result & INTERACT_MENU_RESULT_DEBUG_HARVESTABLES) {
            if (mWorld->getNetMode() == WorldNetMode::Host) {
                TileHandle tileHandle = mRightClickInteractPopup->getSelectedTileHandle();
                tileHandle.container->getHarvestables().debugDraw();
            }
            else {
                assert(false);
            }
        }
        else if (result & INTERACT_MENU_RESULT_DEBUG_PATH_TO_WOOD) {
            if (mSelectedTileHandle.isValid()) {
                GameThreadTasks::getInstance().addGenericTask([](GameThread&, void* worldPtr) {
                    World* world = static_cast<World*>(worldPtr);
                    IEntityComponentSystem& ecs = world->getECS();
                    PhysicsComponent& physCmp = ecs.mRegistry.get<PhysicsComponent>(ecs.getLocalPlayer());
                    NavigationComponent& cmp = ecs.mRegistry.get_or_emplace<NavigationComponent>(ecs.getLocalPlayer());
                    cmp.requestCoarsePathToHarvestable(physCmp.getPosition(), TileHarvestable::WOOD, 1024.0f, nullptr);
                }, mWorld);
            }
        }
        static_assert(INTERACT_MENU_RESULT_COUNT == 15, "update");

        // If we had a result, close window
        if (result) {
            mRightClickInteractPopup.reset();
            ImGui::GetIO().WantCaptureKeyboard = false;
            ImGui::GetIO().WantCaptureMouse = false;
        }
    }
}
