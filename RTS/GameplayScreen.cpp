#include "stdafx.h"
#include "GameplayScreen.h"

#include "App.h"

#include <Vorb/math/VorbMath.hpp>
#include <Vorb/ui/InputDispatcher.h>
#include <Vorb/graphics/SpriteFont.h>
#include <Vorb/graphics/TextureCache.h>
#include <glm/gtx/rotate_vector.hpp>

#include "pathfinding/NavThread.h"

#include "DebugRenderer.h"

#include "camera/CameraController.h"

#include "World.h"
#include "resources/TileRepository.h"
#include "world/WorldObjectQuery.h"
#include "Utils.h"

#include "ResourceManager.h"
#include "item/ItemRepository.h"
#include "item/ItemStockpile.h"
#include "item/ItemStockpileRegistry.h"
#include "particles/ParticleSystemManager.h"

#include "physics/PhysicsWorld.h"

#include "rendering/RenderContext.h"

#include "TextureManip.h"
#include "Random.h"

#include "rendering/ChunkRenderer.h"

#include "ui/UIInteractMenuPopup.h"
#include "ui/UIContext.h"

#include <SDL.h>

#include <Vorb/ui/imgui/imgui.h>

#include "options/DebugOptions.h"

constexpr ui32 MAX_TICKS_PER_UPDATE = 3;
constexpr f64 TICK_RATE_MS = 40.0;

#define WRITE_DEBUG_ATLAS 0

GameplayScreen::GameplayScreen(App* const app)
	: IAppScreen<App>(app),
    mResourceManager(Services::ResourceManager::ref()),
    mWorld(std::make_unique<World>()),
    mRenderContext(RenderContext::initInstance(*mWorld, f32v2(m_app->getWindow().getWidth(), m_app->getWindow().getHeight()), static_cast<SDL_Window*>(m_app->getWindow().getHandle())))
{

    UIContext::initInstance(*mWorld, f32v2(m_app->getWindow().getWidth(), m_app->getWindow().getHeight()), static_cast<SDL_Window*>(m_app->getWindow().getHandle()));

    // TODO: Config
    sDebugOptions.mVSYNC = m_app->getWindow().getSwapInterval() == vui::GameSwapInterval::V_SYNC;

    // TODO: This is kinda stupid
    if (WeaponRegistry::s_allWeaponItems.empty()) {
        ArmorRegistry::loadArmors();
        WeaponRegistry::loadWeapons();
        ShieldRegistry::loadShields();
    }

	// Starting time of day to noon
	mWorld->setTimeOfDay(12.0f);

    // TODO: FIX EVIL THINGS
    mCameraController = std::make_unique<CameraController>(m_app->getWindow(), *mWorld);

	// TODO: A battle is just a graph, with connections between units who are engaging. Engaging units do not need to do any area
	// checks, simply distance checks to graph neighbors. When initiating combat, area checks can be stopped.
	// Units simply check the graph and do AI based on what is around them.
	// units use BFS to update the graph when a connection is broken, drawing new connections as needed.
	// Unit can simulate every single frame since its merely checking a few neighbor pointers, but these are cache misses.
	// Try do group things spatially so cache misses are few. Allocate a single buffer.
}


GameplayScreen::~GameplayScreen() {
}

i32 GameplayScreen::getNextScreen() const {
	return 0;
}

i32 GameplayScreen::getPreviousScreen() const {
	return 0;
}

void GameplayScreen::build() {

	const f32v2 screenSize(m_app->getWindow().getWidth(), m_app->getWindow().getHeight());

    mResourceManager.gatherFiles("data");
	mResourceManager.loadFiles();

    {
        ScopedTimer timer("Render context init");
        mRenderContext.initPostLoad();
    }
#if WRITE_DEBUG_ATLAS == 1
    {
        ScopedTimer timer("Write debug atlas");
        mResourceManager.writeDebugAtlas();
    }
#endif
    {
        ScopedTimer timer("World init");
        mWorld->initPostLoad(mRenderContext.getChunkRenderer().getMesher());
    }

	vui::InputDispatcher::key.onKeyDown.addFunctor([this](Sender sender, const vui::KeyEvent& event) {
		// View toggle
		if (event.keyCode == VKEY_B) {
			sDebugOptions.mWireframe = !sDebugOptions.mWireframe;
        } else if (event.keyCode == VKEY_C) {
            sDebugOptions.mChunkBoundaries = !sDebugOptions.mChunkBoundaries;
        }
        else if (event.keyCode == VKEY_V) {
            sDebugOptions.mCities = !sDebugOptions.mCities;
        }
        else if (event.keyCode == VKEY_J) {
            sDebugOptions.mShowNavGraphUpdates = !sDebugOptions.mShowNavGraphUpdates;
        }
        else if (event.keyCode == VKEY_R/* && vui::InputDispatcher::key.isKeyPressed(VKEY_LALT)*/) { // TODO: Broken on laptop (Nvidia alt + r overlay?)
			mResourceManager.reloadMaterials();
        }
        else if (event.keyCode == VKEY_N) {
			mRenderContext.selectNextDebugShader();
        }
        else if (event.keyCode == VKEY_L) {
			auto&& ecs = mWorld->getECS();
			if (ecs.mRegistry.try_get<DynamicLightComponent>(ecs.mPlayerEntity)) {
				// Remove existing
				ecs.mRegistry.remove<DynamicLightComponent>(ecs.mPlayerEntity);
			} else {
				// Add new
				ecs.mRegistry.emplace<DynamicLightComponent>(ecs.mPlayerEntity);
			}
		}
        else if (event.keyCode == VKEY_Y) {
			sDebugOptions.mShowTweaker = !sDebugOptions.mShowTweaker;
        }
        else if (event.keyCode == VKEY_T) {
            sDebugOptions.mShowEditor = !sDebugOptions.mShowEditor;
        }
        else if (event.keyCode == VKEY_U) {
            if (sDebugOptions.mCameraMode == CameraMode::MMO) {
                sDebugOptions.mCameraMode = CameraMode::FIRST_PERSON;
            }
            else {
                sDebugOptions.mCameraMode = CameraMode::MMO;
            }
        }
        else if (event.keyCode == VKEY_F) {
            if (sDebugOptions.mCameraMode == CameraMode::FREE_LOOK) {
                sDebugOptions.mCameraMode = CameraMode::MMO;
            }
            else {
                sDebugOptions.mCameraMode = CameraMode::FREE_LOOK;
            }
        }
	});

	vui::InputDispatcher::mouse.onButtonDown.addFunctor([this](Sender sender, const vui::MouseButtonEvent& event) {
        UIContext::getInstance().closeTileInspectionPanel();
        if (event.button == vorb::ui::MouseButton::RIGHT) {
            mRightClickTimer.start();
            const f32v3 camPos = mCameraController->getOwnedCamera().getPosition();
            PhysHitResult hitResult = mWorld->getPhysicsWorld().pick(camPos, camPos + sDebugOptions.mMousePickRay * 3000.0f, PICK_TYPE_ALL);
            if (hitResult.didHit()) {
                mRightClickPickPos = hitResult.mPosition;
            }
            else {
                mRightClickPickPos = f32v3(FLT_MAX);
            }
        }
	});

	vui::InputDispatcher::mouse.onMotion.addFunctor([this](Sender sender, const vui::MouseMotionEvent& event) {
		mMousePosition.x = (f32)event.x;
		mMousePosition.y = (f32)event.y;

        // Uncomment for pathfind stress test
       /* TerrainPickData pickData = mWorld->getWorldGrid().pickTerrainFromCameraVector(*mCamera3D, sDebugOptions.mMousePickRay);
        if (pickData.hit.didHit()) {

            NavigationComponent& cmp = mWorld->getECS().mRegistry.get_or_emplace<NavigationComponent>(ecs.mPlayerEntity);
            const PhysicsComponent& physCmp = mWorld->getECS().mRegistry.get<PhysicsComponent>(ecs.mPlayerEntity);
            const f32v2& playerXYPos = physCmp.getXYPosition();
            cmp.requestCoarsePath(ui16v2(pickData.hit.position.x, pickData.hit.position.y), playerXYPos);

        }*/
	});

	vui::InputDispatcher::mouse.onButtonUp.addFunctor([this](Sender sender, const vui::MouseButtonEvent& event) {
		constexpr float VEL_MULT = 0.0001f;
		constexpr float VEL_EXP = 0.4f;
		const f32v2 screenPos(event.x, event.y);

		entt::entity newActor = INVALID_ENTITY;
		if (event.button == vui::MouseButton::LEFT) {
            /*newActor = mUndeadActorFactory->createActor(
                mTestClick,
                vio::Path("data/textures/circle_dir.png"),
                vio::Path("")
            );*/

			if (vui::InputDispatcher::key.isKeyPressed(VKEY_T)) {
                // Teleport
                auto&& ecs = mWorld->getECS();
				if (PhysicsComponent* phys = ecs.mRegistry.try_get<PhysicsComponent>(ecs.mPlayerEntity)) {
                    const f32v3 camPos = mCameraController->getOwnedCamera().getPosition();
                    PhysHitResult hitResult = mWorld->getPhysicsWorld().pick(camPos, camPos + sDebugOptions.mMousePickRay * 3000.0f, PICK_TYPE_ALL);
					if (hitResult.didHit()) {
                        phys->teleportToPoint(hitResult.mPosition);
					}
				}
			}
			else if (vui::InputDispatcher::key.isKeyPressed(VKEY_Q)) {
            /*    TileHandle handle = mWorld->getTileHandleAtWorldPos(worldPos);
                if (handle.isValid()) {
                    Chunk* chunk = handle.getMutableChunk();
                    ui8 height = chunk->getTileAt(handle.index).groundZPosition + 5;
                    chunk->setTileAt(handle.index, Tile(TileRepository::getTile("rock1"), TILE_ID_NONE, TILE_ID_NONE, height));
                }*/
			}
            else if (vui::InputDispatcher::key.isKeyPressed(VKEY_E)) {
                /*TileHandle handle = mWorld->getTileHandleAtWorldPos(worldPos);
                if (handle.isValid()) {
                    Chunk* chunk = handle.getMutableChunk();
                    ui8 height = chunk->getTileAt(handle.index).groundZPosition;
                    chunk->setTileAt(handle.index, Tile(TileRepository::getTile("rock1"), TILE_ID_NONE, TILE_ID_NONE, height));
                }*/
            }
            else if (vui::InputDispatcher::key.isKeyPressed(VKEY_C)) {
                /*TileHandle handle = mWorld->getTileHandleAtWorldPos(worldPos);
                mWorld->createCityAt(ui32v2(floor(worldPos.x), floor(worldPos.y)));*/
            }
			else {
				if (mRightClickInteractPopup) {
					mRightClickInteractPopup.reset();
				}
			}
        }
        else if (event.button == vui::MouseButton::RIGHT) {
			if (vui::InputDispatcher::key.isKeyPressed(VKEY_P)) {
                /*const f32v3 pos(worldPos.x, worldPos.y, 0.5f);
                mResourceManager.getParticleSystemManager().createParticleSystem(pos, f32v3(1.0f, 0.0f, 0.0f), "blood");*/
			}
			else if (vui::InputDispatcher::key.isKeyPressed(VKEY_G)) {
                /*mWorld->createEntity(worldPos, "villager");*/
			}
            else {
                constexpr f64 RIGHT_CLICK_INTERACT_MS_THRESHOLD = 160.0;
                if (mRightClickInteractPopup) {
                    mRightClickInteractPopup.reset();
				}
                else if (mRightClickTimer.stop() < RIGHT_CLICK_INTERACT_MS_THRESHOLD) {
                    const f32v3 camPos = mCameraController->getOwnedCamera().getPosition();
                    PhysHitResult hitResult = mWorld->getPhysicsWorld().pick(camPos, camPos + sDebugOptions.mMousePickRay * 3000.0f, PICK_TYPE_ALL);
                    if (hitResult.didHit()) {
                        // For interact must click in about the same spot
                        if (glm::length(mRightClickPickPos - hitResult.mPosition) < 0.05f) {
                            f32v2 worldPos = f32v2(hitResult.mPosition.x, hitResult.mPosition.y);
                            WorldObjectQuery worldObjectQuery(*mWorld, worldPos);
                            // Right click picking
                            mSelectedTilePosition = worldPos;
                            // Enable context menu
                            mSelectedScreenPos = screenPos;
                            mRightClickInteractPopup = std::make_unique<UIInteractMenuPopup>(screenPos, static_cast<SDL_Window*>(m_app->getWindow().getHandle()), std::move(worldObjectQuery));
                        }
                    }
				}
			}
		}

		// Apply velocity
		if (newActor != INVALID_ENTITY) {
            /*auto& physcomp = mecs->getphysicscomponentfromentity(newactor);
            velocity = velocity;
            physcomp.mbody->applyforce(reinterpret_cast<b2vec2&>(velocity), physcomp.mbody->getworldcenter(), true);*/
		}
	});


    // Add player
    f32v3 playerPos(WorldData::WORLD_CENTER.x, WorldData::WORLD_CENTER.y, 20.0f);
    mWorld->getWorldGrid().tryComputeHeightAtPoint(playerPos, &playerPos.z);
    auto&& ecs = mWorld->getECS();
    ecs.mPlayerEntity = mWorld->createEntity(playerPos, "player");
    assert((ui32)ecs.mPlayerEntity != (ui32)INVALID_ENTITY);

    mCameraController->setEntityFollow(ecs.mPlayerEntity);
}

void GameplayScreen::destroy(const vui::GameTime& gameTime) {
	
}

void GameplayScreen::onEntry(const vui::GameTime& gameTime) {
    // Hacky load screen
    {
        ScopedTimer timer("Main thread preload hack");
        update(gameTime);
        while (Services::Threadpool::ref().getTasksSizeApprox()) {
            Sleep(1);
            update(gameTime);
        }
        std::cout << "\n DONE\n";
    }
}

void GameplayScreen::onExit(const vui::GameTime& gameTime) {
}

void GameplayScreen::update(const vui::GameTime& gameTime) {

	mGameTimer.startFrame();

    updateTimeScaling(gameTime);

    // Update main thread update queues
    mWorld->updateTaskQueues();

    // Update the world with fixed timestep
    int ticks = 0;
    auto&& ecs = mWorld->getECS();
	while (mGameTimer.tryTick() && ticks++ < MAX_TICKS_PER_UPDATE) {

        const PhysicsComponent& playerPhysCmp = ecs.mRegistry.get<PhysicsComponent>(ecs.mPlayerEntity);
        f32v3 position = playerPhysCmp.getPosition();
        mWorld->tick(position);

	}

    // Update editors
    UIContext::getInstance().updateEditors(mCameraController->getOwnedCamera());


	updateTilePicking();
    mWorld->frameUpdate(mCameraController->getOwnedCamera(), gameTime.elapsedSec);

    // TODO: Actual usage of deltatime?
    mCameraController->update(gameTime, mGameTimer.getFrameAlpha());

}

void GameplayScreen::draw(const vui::GameTime& gameTime) {

	const f32 frameAlpha = mGameTimer.getFrameAlpha();

    // Grab fps
    sFps = vmath::lerp(sFps, m_app->getFps(), 0.85f);
    mFps = sFps;

    auto&& ecs = mWorld->getECS();
	PhysicsComponent& cmp = ecs.mRegistry.get<PhysicsComponent>(ecs.mPlayerEntity);
    const f32v3 playerPos = cmp.getInterpolatedPosition();
	mRenderContext.renderFrame(mCameraController->getOwnedCamera(), playerPos, frameAlpha, gameTime.elapsedSec);

	tryUpdateAndRenderInteractPopup((const f32v2&)playerPos);

	mRenderContext.endFrame();

}

void GameplayScreen::updateTimeScaling(const vui::GameTime& gameTime) {
    // DEBUG Time advance
    static constexpr float TIME_ADVANCE_MULT = 4.0f;
    if (vui::InputDispatcher::key.isKeyPressed(VKEY_LEFT)) {
        if (vui::InputDispatcher::key.isKeyPressed(VKEY_LSHIFT)) {
            sDebugOptions.mTimeOffset -= gameTime.elapsedSec * 250.0f;
        }
        else {
            sDebugOptions.mTimeOffset -= gameTime.elapsedSec * TIME_ADVANCE_MULT;
        }
        mGameTimer.setMsPerTick(MS_PER_GAME_TICK / 2.0f);
    }
    else if (vui::InputDispatcher::key.isKeyPressed(VKEY_RIGHT)) {
        if (vui::InputDispatcher::key.isKeyPressed(VKEY_LSHIFT)) {
            sDebugOptions.mTimeOffset += gameTime.elapsedSec * 250.0f;
        }
        else {
            sDebugOptions.mTimeOffset += gameTime.elapsedSec * TIME_ADVANCE_MULT;
        }
        mGameTimer.setMsPerTick(MS_PER_GAME_TICK / 2.0f);
    }
    else {
        mGameTimer.setMsPerTick(MS_PER_GAME_TICK);
    }
}

void GameplayScreen::updateTilePicking() {
	const f32 normalizedX = (mMousePosition.x / (f32)m_app->getWindow().getWidth()) * 2.0f - 1.0f;
	const f32 normalizedY = -((mMousePosition.y / (f32)m_app->getWindow().getHeight()) * 2.0f - 1.0f);
	f32v4 pickRayClipSpace(normalizedX, normalizedY, -1.0f, 1.0f);
	f32v4 pickRayEyeSpace = glm::inverse(mCameraController->getOwnedCamera().getProjectionMatrix()) * pickRayClipSpace;
	pickRayEyeSpace.z = -1.0f;
	pickRayEyeSpace.w = 0.0f;
	f32v4 pickRayWorldSpace = glm::inverse(mCameraController->getOwnedCamera().getViewMatrix()) * pickRayEyeSpace;
	f32v3 pickRayXYZ(pickRayWorldSpace.x, pickRayWorldSpace.y, pickRayWorldSpace.z);
	sDebugOptions.mMousePickRay = glm::normalize(pickRayXYZ);
}


void GameplayScreen::tryUpdateAndRenderInteractPopup(const f32v2& playerPos) {
    // Handle interact menu TODO: Notify to get this out of here
    if (mRightClickInteractPopup) {
        // Render selected
        const ui32v2 worldPosInt = mSelectedTilePosition;
        const f32 height = mWorld->getWorldGrid().tryComputeHeightAtPoint(f32v2(worldPosInt) + f32v2(0.5f));
        DebugRenderer::drawFilledQuad(f32v3(worldPosInt.x, worldPosInt.y, height), f32v2(1.0f), color4(1.0f, 1.0f, 0.0f, 0.5f));

        const UIInteractMenuResultFlags result = mRightClickInteractPopup->updateAndRender();
        // TODO: Notify
        if (result & INTERACT_MENU_RESULT_PATHFIND) {
            auto&& ecs = mWorld->getECS();
            NavigationComponent& cmp = ecs.mRegistry.get_or_emplace<NavigationComponent>(ecs.mPlayerEntity);
            cmp.requestCoarsePath(PathPoint(playerPos), PathPoint(worldPosInt));
        }
        else if (result & INTERACT_MENU_RESULT_CLEAR_TILE) {
            // grass
            TileHandle handle = mWorld->getTileHandleAtWorldPos(mSelectedTilePosition);
			// TODO: HANDLE RACE CONDITION
            if (handle.isValid()) {
                handle.getMutableContainer()->setTileAt(handle.tileIndex, Tile(TileRepository::getTile("grass1"), TILE_ID_NONE, TILE_ID_NONE));
            }
        }
        else if (result & INTERACT_MENU_RESULT_PLANT_TREE) {
            // grass
            TileHandle handle = mWorld->getTileHandleAtWorldPos(mSelectedTilePosition);
            if (handle.isValid()) {
                handle.getMutableContainer()->setTileAt(handle.tileIndex, Tile(TileRepository::getTile("grass1"), TILE_ID_NONE, TileRepository::getTile("tree_small")));
            }
        }
        else if (result & INTERACT_MENU_RESULT_PLANT_TREE_2) {
            // grass
            TileHandle handle = mWorld->getTileHandleAtWorldPos(mSelectedTilePosition);
            if (handle.isValid()) {
                handle.getMutableContainer()->setTileAt(handle.tileIndex, Tile(TileRepository::getTile("grass1"), TILE_ID_NONE, TileRepository::getTile("tree_pine")));
            }
        }
        else if (result & INTERACT_MENU_RESULT_BUILD_WALL) {
            // grass
            TileHandle handle = mWorld->getTileHandleAtWorldPos(mSelectedTilePosition);
            if (handle.isValid()) {
                handle.getMutableContainer()->setTileAt(handle.tileIndex, Tile(TileRepository::getTile("rock1"), TILE_ID_NONE, TILE_ID_NONE, 2u));
            }
        }
        else if (result & INTERACT_MENU_RESULT_INSPECT) {
            // grass
            TileHandle handle = mWorld->getTileHandleAtWorldPos(mSelectedTilePosition);

            UIContext::getInstance().activateTileInspectionPanel(mSelectedScreenPos, handle);
        }
        else if (result & INTERACT_MENU_RESULT_DEBUG_ADD_25_WOOD) {
            // grass
			WorldObjectQuery& worldObjects = mRightClickInteractPopup->getWorldObjects();
			ItemStockpile* stockPile = worldObjects.getStockpile();
			assert(stockPile);
			ItemStack woodPile;
			woodPile.id = mResourceManager.getItemRepository().getItem("wood_raw").getID();
			woodPile.quantity = 25;
            if (std::unique_ptr<ItemReservation> itemPromise = stockPile->tryPromiseItemStack(woodPile, 1)) {
                while (!itemPromise->isFinished()) {
                    itemPromise->fulfillCurrentTarget(woodPile);
                }
            }
        }
        else if (result & INTERACT_MENU_RESULT_DEBUG_DESTROY_STOCK) {
            // grass
            WorldObjectQuery& worldObjects = mRightClickInteractPopup->getWorldObjects();
            ItemStockpile* stockPile = worldObjects.getStockpile();
			mWorld->getItemStockpileRegistry().destroyStockpile(stockPile);
        }
        else if (result & INTERACT_MENU_RESULT_DEBUG_KILL_AGENT) {
            assert(false);
        }
        else if (result & INTERACT_MENU_RESULT_DEBUG_PATH_ROOM) {
            const RoomNode* selectedNode = mRightClickInteractPopup->tryGetSelectedRoom();
            assert(selectedNode);
            const Building* building = mRightClickInteractPopup->tryGetSelectedBuilding();
            assert(building);
            const ui32AABB3& aabb = building->getAABB();
            ui32v2 roomWorldPos = ui32v2(selectedNode->offsetFromZero) + ui32v2(aabb.x, aabb.y);

            // TODO: Closest entrance?
            //RoomNodeID entrance = building->getNavEntrances().begin()->second;
            TileIndex id = building->getNavEntrances().begin()->first;
            const ui32v3 targetPos = building->getWorldPositionOfTile(id);
            // TODO: Path into the actual room
            auto&& ecs = mWorld->getECS();
            NavigationComponent& cmp = ecs.mRegistry.get_or_emplace<NavigationComponent>(ecs.mPlayerEntity);
            cmp.requestCoarsePathWithCallback(PathPoint(playerPos), PathPoint(targetPos), [this](bool success) {
                if (success) {
                    assert(false);
                }
                assert(false);
            });
        }
        static_assert(INTERACT_MENU_RESULT_COUNT == 11, "update");

        // If we had a result, close window
        if (result) {
            mRightClickInteractPopup.reset();
            ImGui::GetIO().WantCaptureKeyboard = false;
            ImGui::GetIO().WantCaptureMouse = false;
        }
    }
}