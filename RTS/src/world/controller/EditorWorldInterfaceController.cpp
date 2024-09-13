#include "stdafx.h"
#include "EditorWorldInterfaceController.h"

#include "tile/TileContainer.h"
#include "camera/CameraController.h"
#include "gamethread/GameThreadTasks.h"
#include "resources/ResourceManager.h"
#include "item/ItemRepository.h"
#include "item/ItemStockpile.h"
#include "item/ItemStockpileRegistry.h"
#include "rendering/RenderContext.h"
#include "world/World.h"
#include "ecs/IFullECS.h"
#include "ecs/factory/EntityFactory.h"
#include "ui/UIContext.h"
#include "pathfinding/NavWorld.h"
#include "world/chunk/SimChunkGrid.h"
#include "effect/IEffectContext.h"

#include "resources/TileRepository.h"

#include <imgui.h>
#include "ui/GameWindow.h"

#include "options/DebugOptions.h"

#include "physics/PhysicsWorld.h"
#include "physics/PhysicsBroadPhaseLayerFilters.h"

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

    static bool wasMPressed = false;
    static bool wasOPressed = false;
    if (vui::InputDispatcher::key.isKeyDown(VKEY_Y)) {
        // TODO: ITEMFactory
        GameThreadTasks::getInstance().addGenericTask([this]() {
            ItemID id = Random::getCachedRandom() % 2 ? ItemRepository::get().getAssetID(CStrToken("wood_log")) : ItemRepository::get().getAssetID(CStrToken("wood_log_birch"));
            ItemStack newStack(id, 1);
            f32v3 velocity = f32v3(Random::getCachedRandomf() * 2.0f - 1.0f, Random::getCachedRandomf() * 2.0f - 1.0f, 4.0f);
            EntityFactory::createItemProjectile(*mWorld, mWorld->getECS().getLocalPlayerPosition() + f32v3(0.0f, 0.0f, 1.0f), velocity, newStack);
            sDebugOptions.mCities = !sDebugOptions.mCities;
        });
    }
    if (vui::InputDispatcher::key.isKeyDown(VKEY_O)) {
        if (!wasOPressed) {
            GameThreadTasks::getInstance().addGenericTask([this]() {
                for (int i = 0; i < 50; ++i) {
                    mWorld->getEffectContext().playParticleEffectAtPoint(
                        EffectAssetRef(CStrToken("snow")), mWorld->getECS().getLocalPlayerPosition(), nullptr, BitFlags<EffectCreateFlags>()
                    );
                }
            });
        }
        wasOPressed = true;
    }
    else {
        wasOPressed = false;
    }
    if (vui::InputDispatcher::key.isKeyDown(VKEY_M)) {
        // Test item container
        if (!wasMPressed) {

            GameThreadTasks::getInstance().addGenericTask([this]() {
                ItemStack stack1(ItemRepository::get().getAssetID(CStrToken("wood_log")), 15);
                ItemStack stack2(ItemRepository::get().getAssetID(CStrToken("wood_log_birch")), 15);

                TileItemUID uid1 = mWorld->getSimChunkGrid().tryDropItemStackOnGroundGameThread(stack1, mWorld->getECS().getLocalPlayerPosition(), false /*creatEntity*/);
                TileItemUID uid2 = mWorld->getSimChunkGrid().tryDropItemStackOnGroundGameThread(stack2, mWorld->getECS().getLocalPlayerPosition(), false /*creatEntity*/);

                TileIndex index = mWorld->getTileHandleAtWorldPos(mWorld->getECS().getLocalPlayerPosition()).tileIndex;
                TileItemStack stacks[2];
                stacks[0] = TileItemStack(stack1, index, uid1);
                stacks[1] = TileItemStack(stack2, index, uid2);

                EntityFactory::createItemContainerOnGround(*mWorld, mWorld->getECS().getLocalPlayerPosition(), std::span<TileItemStack>(stacks, 2));
            });
            wasMPressed = true;
        }
    }
    else {
        wasMPressed = false;
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

    if (mRightClickDownPick) {
        PhysHitResult hitResult = *mRightClickDownPick;
        if (hitResult.didHit()) {
            mRightClickPickPos = hitResult.mPosition;
        }
        else {
            mRightClickPickPos = f32v3(FLT_MAX);
        }
        mRightClickDownPick.reset();
    }
    if (mRightClickUpPick) {
        PhysHitResult hitResult = *mRightClickUpPick;
        if (hitResult.didHit()) {

            // TMP REMOVE
            if (vui::InputDispatcher::key.isKeyDown(VKEY_I)) {
                AM::DebugRenderer::drawWireQuad(hitResult.mPosition + f32v3(0.0f, 0.0f, 0.5f), f32v2(0.3f), color4(1.0f, 0.0f, 0.0f, 1.0f), 100);
                mWorld->getEffectContext().playParticleEffectAtPoint(EffectAssetRef(CStrToken("hitfx")), hitResult.mPosition + f32v3(0.0f, 0.0f, 0.5f), nullptr, BitFlags<EffectCreateFlags>());
                mRightClickUpPick.reset();
                return;
            }

            mSelectedScreenPos = mRightClickUpPickScreenPos;
            // For interact must click in about the same spot
            PhysicsBodyUserData bodyUserData = hitResult.mBodyUserData;
            PhysicsBodyUserDataType type = bodyUserData.getType();
            if (type == PhysicsBodyUserDataType::Entity) {
                LOG_CRITICAL("Selected Entity");
            }
            else if (type == PhysicsBodyUserDataType::Tile) {
                if (glm::length(mRightClickPickPos - hitResult.mPosition) < 0.05f) {
                    const auto& [containerOwner, index] = bodyUserData.getTileData();
                    // ONLY WORKS FOR MODELS
                    if (index != INVALID_TILE_INDEX) {
                        // Query whatever we selected
                        mWorldObjectQuery = WorldObjectQueryFactory::makeQuery(*mWorld, LiteTileHandle(containerOwner, index));
                        mIsQuerying = true;
                    }
                }
            } else if (type == PhysicsBodyUserDataType::Terrain) {
                f32v3 worldPos = hitResult.mPosition + hitResult.mNormal * 0.01f;
                mWorldObjectQuery = WorldObjectQueryFactory::makeQuery(*mWorld, worldPos);
                mIsQuerying = true;
            }
        }
        mRightClickUpPick.reset();
    }
}


void EditorWorldInterfaceController::initEvents() {
    vui::InputDispatcher::key.registerKeyListeners(mKeyListeners);
    vui::InputDispatcher::mouse.registerMouseListeners(mMouseListeners);

    vui::InputDispatcher::key.addKeyDownListener(mKeyListeners, [this](const vui::KeyEvent& event) {

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
        else if (event.keyCode == VKEY_R && event.mod.lShift) {
            Services::ResourceManager::ref().reloadMaterials();
        }
        else if (event.keyCode == VKEY_N && event.mod.lShift) {
            RenderContext::getInstance().selectNextDebugShader();
        }
        else if (event.keyCode == VKEY_U) {
            if (sDebugOptions.mCameraMode == CameraMode::MMO) {
                sDebugOptions.mCameraMode = CameraMode::FIRST_PERSON;
                GameThreadTasks::getInstance().addHideLocalPlayerModelTask(*mWorld, true);
            }
            else {
                sDebugOptions.mCameraMode = CameraMode::MMO;
                GameThreadTasks::getInstance().addHideLocalPlayerModelTask(*mWorld, false);
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
            LOG_WARN("TODO: Implement physics profiling");
            /*   GameThreadTasks::getInstance().addGenericTask([world = mWorld]() {
                   if (world->getPhysicsWorld().isProfiling()) {
                       world->getPhysicsWorld().endB3ProfilingAndDumpToFile("bullet_timings");
                   }
                   else {
                       world->getPhysicsWorld().startB3Profiling();
                   }
               });*/
        }
        else if (event.keyCode == VKEY_ESCAPE) {
            UIContext::getInstance().toggleEscapeMenu();
        }
        else if (event.keyCode == VKEY_F1 && event.mod.lShift && (event.mod.lAlt || event.mod.lCtrl)) {
            UIContext::getInstance().toggleGameplayDebugger();
        }
        else if (event.keyCode == VKEY_5) {
            sDebugOptions.mShowEditor = !sDebugOptions.mShowEditor;
            // Notify panels that we are losing or gaining context
            if (sDebugOptions.mShowEditor) {
                UIContext::getInstance().onEditorOpen();
            }
            else {
                UIContext::getInstance().onEditorClose();
            }
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
                mRightClickDownPick = std::make_unique<PhysHitResult>();
                const f32v3 camPos = mCameraController->getOwnedCamera().getPosition();
                *mRightClickDownPick = mWorld->getPhysicsWorld().raycastFirst(camPos, camPos + mMousePickRay * 3000.0f, PhysicsBroadphaseLayerFilterStatic());
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
            if (vui::InputDispatcher::key.isKeyDown(VKEY_T)) {
                // Teleport
                GameThreadTasks::getInstance().addCameraPickTeleportTask(*mWorld, mCameraController->getOwnedCamera().getPosition(), mMousePickRay);
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
                mRightClickUpPick = std::make_unique<PhysHitResult>();
                *mRightClickUpPick = mWorld->getPhysicsWorld().raycastFirst(camPos, camPos + mMousePickRay * 3000.0f, PhysicsBroadphaseLayerFilterStatic());
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
                GameThreadTasks::getInstance().addGenericTask([data = taskData]() {
                    // TODO: Small race condition here if tile handle changes or chunk is destroyed
                    const TileHandle& tileHandle = data->second;
                    if (tileHandle.isValid()) {
                        IFullECS& ecs = data->first->getECS();
                        PhysicsComponent& physCmp = ecs.mRegistry.get<PhysicsComponent>(ecs.getLocalPlayer());
                        NavigationComponent& cmp = ecs.mRegistry.get_or_emplace<NavigationComponent>(ecs.getLocalPlayer());
                        cmp.requestCoarsePath(physCmp.getBottomPosition(), tileHandle.getWorldPos3D(), 5.0f);
                    }
                    delete data;
                });
            }
        }
        else if (result & INTERACT_MENU_RESULT_CLEAR_TILE) {
            if (mSelectedTileHandle.isValid()) {
                TileHandle* tileHandlePtr = new TileHandle(mSelectedTileHandle);
                GameThreadTasks::getInstance().addGenericTask([tileHandlePtr]() {
                    TileContainer* container = tileHandlePtr->getMutableContainer();
                    container->setTile(tileHandlePtr->tileIndex, TILE_ID_NONE, 0);
                    delete tileHandlePtr;
                });
            }
        }
        else if (result & INTERACT_MENU_RESULT_PLANT_TREE) {
            if (mSelectedTileHandle.isValid()) {
                TileHandle* tileHandlePtr = new TileHandle(mSelectedTileHandle);
                GameThreadTasks::getInstance().addGenericTask([tileHandlePtr]() {
                    tileHandlePtr->getMutableContainer()->setTile(tileHandlePtr->tileIndex, TileRepository::get().getTileID(CStrToken("tree_a")), 0);
                    delete tileHandlePtr;
                });
            }
        }
        else if (result & INTERACT_MENU_RESULT_PLANT_TREE_2) {
            if (mSelectedTileHandle.isValid()) {
                TileHandle* tileHandlePtr = new TileHandle(mSelectedTileHandle);
                GameThreadTasks::getInstance().addGenericTask([tileHandlePtr]() {
                    tileHandlePtr->getMutableContainer()->setTile(tileHandlePtr->tileIndex, TileRepository::get().getTileID(CStrToken("bush_med")), 0);
                    delete tileHandlePtr;
                });
            }
        }
        else if (result & INTERACT_MENU_RESULT_BUILD_WALL) {
            if (mSelectedTileHandle.isValid()) {
                TileHandle* tileHandlePtr = new TileHandle(mSelectedTileHandle);
                GameThreadTasks::getInstance().addGenericTask([tileHandlePtr]() {
                    tileHandlePtr->getMutableContainer()->setTile(tileHandlePtr->tileIndex, TileRepository::get().getTileID(CStrToken("rock1")), 0);
                    delete tileHandlePtr;
                });
            }
        }
        else if (result & INTERACT_MENU_RESULT_INSPECT) {
            // grass
            UIContext::getInstance().activateTileInspectionPanel(mSelectedScreenPos, mSelectedTileHandle);
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
                mWorld->tryGetNavWorld()->debugDrawCoarseNavGraphForContainer(*tileHandle.container, 2000);
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
                mWorld->tryGetNavWorld()->debugDrawCoarseNavNode(tileHandle, 2000);
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
                GameThreadTasks::getInstance().addGenericTask([world = mWorld]() {
                    IFullECS& ecs = world->getECS();
                    PhysicsComponent& physCmp = ecs.mRegistry.get<PhysicsComponent>(ecs.getLocalPlayer());
                    NavigationComponent& cmp = ecs.mRegistry.get_or_emplace<NavigationComponent>(ecs.getLocalPlayer());
                    cmp.requestCoarsePathToHarvestable(physCmp.getBottomPosition(), TileHarvestable::Wood, 1024.0f);
                });
            }
        }
        else if (result & INTERACT_MENU_RESULT_REBUILD_NAVMESH) {
            std::pair<TileHandle, World*>* taskData = new std::pair<TileHandle, World*>(mSelectedTileHandle, mWorld);
            TileHandle* tileHandlePtr = new TileHandle(mSelectedTileHandle);
            GameThreadTasks::getInstance().addGenericTask([taskData]() {
                World* world = static_cast<World*>(taskData->second);
                if (NavWorld* navWorld = world->tryGetNavWorld()) {
                    navWorld->markContainerNavDirty(taskData->first.getMutableContainer());
                }
            });
        }
        else if (result & INTERACT_MENU_RESULT_TRANSFORM_TILE) {
            if (mSelectedTileHandle.isValid()) {
                TileHandle* tileHandlePtr = new TileHandle(mSelectedTileHandle);
                GameThreadTasks::getInstance().addGenericTask([tileHandlePtr]() {
                    TileContainer* container = tileHandlePtr->getMutableContainer();
                    container->tryTransformTile(tileHandlePtr->tileIndex, TileTransformationType::CCorrupt);
                    delete tileHandlePtr;
                });
            }
        }
        static_assert(INTERACT_MENU_RESULT_COUNT == 17, "update");

        // If we had a result, close window
        if (result) {
            mRightClickInteractPopup.reset();
            ImGui::GetIO().WantCaptureKeyboard = false;
            ImGui::GetIO().WantCaptureMouse = false;
        }
    }
}
