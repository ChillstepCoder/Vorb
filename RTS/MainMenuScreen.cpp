#include "stdafx.h"
#include "MainMenuScreen.h"

#include "App.h"

#include <Vorb/math/VorbMath.hpp>
#include <Vorb/ui/InputDispatcher.h>
#include <Vorb/graphics/SpriteFont.h>
#include <Vorb/graphics/TextureCache.h>
#include <glm/gtx/rotate_vector.hpp>

#include "pathfinding/NavThread.h"

#include "DebugRenderer.h"

#include <box2d/b2_body.h>
#include <box2d/b2_contact.h>

#include "camera/Camera3D.h"

#include "World.h"
#include "world/TileRepository.h"
#include "world/WorldObjectQuery.h"
#include "Utils.h"

#include "ResourceManager.h"
#include "item/ItemRepository.h"
#include "item/ItemStockpile.h"
#include "item/ItemStockpileRegistry.h"
#include "particles/ParticleSystemManager.h"
#include "services/Services.h"

#include "physics/ContactListener.h"
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

const f32v2 CAMERA_ZOOM_RANGE = f32v2(1.0f, 1024.0f);

#define WRITE_DEBUG_ATLAS 0

MainMenuScreen::MainMenuScreen(const App* app) 
	: IAppScreen<App>(app),
    mResourceManager(&Services::ResourceManager::ref()),
    mWorld(std::make_unique<World>(*mResourceManager)),
    mRenderContext(RenderContext::initInstance(*mResourceManager, *mWorld, f32v2(m_app->getWindow().getWidth(), m_app->getWindow().getHeight()), static_cast<SDL_Window*>(m_app->getWindow().getHandle())))
{

    UIContext::initInstance(*mWorld, f32v2(m_app->getWindow().getWidth(), m_app->getWindow().getHeight()), static_cast<SDL_Window*>(m_app->getWindow().getHandle()));

    // TODO: Config
    sDebugOptions.mVSYNC = m_app->getWindow().getSwapInterval() == vui::GameSwapInterval::V_SYNC;

    mCamera3D = std::make_unique<Camera3D>();
	
    // TODO: This is kinda stupid
    if (WeaponRegistry::s_allWeaponItems.empty()) {
        ArmorRegistry::loadArmors();
        WeaponRegistry::loadWeapons();
        ShieldRegistry::loadShields();
    }

	// Starting time of day to noon
	mWorld->setTimeOfDay(12.0f);


	// TODO: A battle is just a graph, with connections between units who are engaging. Engaging units do not need to do any area
	// checks, simply distance checks to graph neighbors. When initiating combat, area checks can be stopped.
	// Units simply check the graph and do AI based on what is around them.
	// units use BFS to update the graph when a connection is broken, drawing new connections as needed.
	// Unit can simulate every single frame since its merely checking a few neighbor pointers, but these are cache misses.
	// Try do group things spatially so cache misses are few. Allocate a single buffer.
}


MainMenuScreen::~MainMenuScreen() {
}

i32 MainMenuScreen::getNextScreen() const {
	return 0;
}

i32 MainMenuScreen::getPreviousScreen() const {
	return 0;
}

void MainMenuScreen::build() {

	const f32v2 screenSize(m_app->getWindow().getWidth(), m_app->getWindow().getHeight());

	mCamera3D->init((f32)m_app->getWindow().getWidth() / m_app->getWindow().getHeight());

    mResourceManager->gatherFiles("data");
	mResourceManager->loadFiles();

    {
        ScopedTimer timer("Render context init");
        mRenderContext.initPostLoad();
    }
#if WRITE_DEBUG_ATLAS == 1
    {
        ScopedTimer timer("Write debug atlas");
        mResourceManager->writeDebugAtlas();
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
        else if (event.keyCode == VKEY_R && vui::InputDispatcher::key.isKeyPressed(VKEY_LALT)) {
			mResourceManager->reloadMaterials();
        }
        else if (event.keyCode == VKEY_N) {
			mRenderContext.selectNextDebugShader();
        }
        else if (event.keyCode == VKEY_L) {
			auto&& ecs = mWorld->getECS();
			if (ecs.mRegistry.try_get<DynamicLightComponent>(mPlayerEntity)) {
				// Remove existing
				ecs.mRegistry.remove<DynamicLightComponent>(mPlayerEntity);
			} else {
				// Add new
				ecs.mRegistry.emplace<DynamicLightComponent>(mPlayerEntity);
			}
		}
        else if (event.keyCode == VKEY_Q) {
            mCameraCartesianDirection = CARTESIAN_NEIGHBORS[e_cast(mCameraCartesianDirection)][1];
            mCameraDirectionTweener.mTarget = TARGET_CAMERA_NORMALS_3D[e_cast(mCameraCartesianDirection)];
        }
        else if (event.keyCode == VKEY_E) {
            mCameraCartesianDirection = CARTESIAN_NEIGHBORS[e_cast(mCameraCartesianDirection)][0];
            mCameraDirectionTweener.mTarget = TARGET_CAMERA_NORMALS_3D[e_cast(mCameraCartesianDirection)];
        }
        else if (event.keyCode == VKEY_T) {
			sDebugOptions.mShowTweaker = !sDebugOptions.mShowTweaker;
        }
        else if (event.keyCode == VKEY_Y) {
            sDebugOptions.mShowEditor = !sDebugOptions.mShowEditor;
        }
	});

	vui::InputDispatcher::mouse.onWheel.addFunctor([this](Sender sender, const vui::MouseWheelEvent& event) {
		mCameraPositionTweener.mTarget.z = glm::clamp(mCameraPositionTweener.mTarget.z + event.dy * mCameraPositionTweener.mTarget.z * -0.2f, CAMERA_ZOOM_RANGE.x, CAMERA_ZOOM_RANGE.y);
	});

	vui::InputDispatcher::mouse.onButtonDown.addFunctor([this](Sender sender, const vui::MouseButtonEvent& event) {
        UIContext::getInstance().closeTileInspectionPanel();
	});

	vui::InputDispatcher::mouse.onMotion.addFunctor([this](Sender sender, const vui::MouseMotionEvent& event) {
		mMousePosition.x = (f32)event.x;
		mMousePosition.y = (f32)event.y;

        // Uncomment for pathfind stress test
       /* TerrainPickData pickData = mWorld->getWorldGrid().pickTerrainFromCameraVector(*mCamera3D, sDebugOptions.mMousePickRay);
        if (pickData.hit.didHit()) {

            NavigationComponent& cmp = mWorld->getECS().mRegistry.get_or_emplace<NavigationComponent>(mPlayerEntity);
            const PhysicsComponent& physCmp = mWorld->getECS().mRegistry.get<PhysicsComponent>(mPlayerEntity);
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
				if (PhysicsComponent* phys = ecs.mRegistry.try_get<PhysicsComponent>(mPlayerEntity)) {
                    TerrainPickData pickData = mWorld->getWorldGrid().pickTerrainFromCameraVector(*mCamera3D, sDebugOptions.mMousePickRay);
					if (pickData.hit.didHit()) {
						phys->teleportToPoint(pickData.hit.position);
					}
				}
			}
			else if (vui::InputDispatcher::key.isKeyPressed(VKEY_Q)) {
            /*    TileHandle handle = mWorld->getTileHandleAtWorldPos(worldPos);
                if (handle.isValid()) {
                    Chunk* chunk = handle.getMutableChunk();
                    ui8 height = chunk->getTileAt(handle.index).baseZPosition + 5;
                    chunk->setTileAt(handle.index, Tile(TileRepository::getTile("rock1"), TILE_ID_NONE, TILE_ID_NONE, height));
                }*/
			}
            else if (vui::InputDispatcher::key.isKeyPressed(VKEY_E)) {
                /*TileHandle handle = mWorld->getTileHandleAtWorldPos(worldPos);
                if (handle.isValid()) {
                    Chunk* chunk = handle.getMutableChunk();
                    ui8 height = chunk->getTileAt(handle.index).baseZPosition;
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
                mResourceManager->getParticleSystemManager().createParticleSystem(pos, f32v3(1.0f, 0.0f, 0.0f), "blood");*/
			}
			else if (vui::InputDispatcher::key.isKeyPressed(VKEY_G)) {
                /*mWorld->createEntity(worldPos, "villager");*/
			}
            else {
                if (mRightClickInteractPopup) {
                    mRightClickInteractPopup.reset();
				}
                else {
                    TerrainPickData pickData = mWorld->getWorldGrid().pickTerrainFromCameraVector(*mCamera3D, sDebugOptions.mMousePickRay);
                    if (pickData.hit.didHit()) {
                        f32v2 worldPos = f32v2(pickData.hit.position.x, pickData.hit.position.y);
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

		// Apply velocity
		if (newActor != INVALID_ENTITY) {
            /*auto& physcomp = mecs->getphysicscomponentfromentity(newactor);
            velocity = velocity;
            physcomp.mbody->applyforce(reinterpret_cast<b2vec2&>(velocity), physcomp.mbody->getworldcenter(), true);*/
		}
	});

	// Add player
	mPlayerEntity = mWorld->createEntity(WorldData::WORLD_CENTER, "player");
	assert((ui32)mPlayerEntity != (ui32)INVALID_ENTITY);
    mCamera3D->setPosition(f32v3(WorldData::WORLD_CENTER.x, 2.0f, WorldData::WORLD_CENTER.y));
	mCameraPositionTweener = f32v3(WorldData::WORLD_CENTER.x, WorldData::WORLD_CENTER.y, 5.0f);

}

void MainMenuScreen::destroy(const vui::GameTime& gameTime) {
	
}

void MainMenuScreen::onEntry(const vui::GameTime& gameTime) {
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

void MainMenuScreen::onExit(const vui::GameTime& gameTime) {
}

void MainMenuScreen::update(const vui::GameTime& gameTime) {

	mGameTimer.startFrame();

    // Store camera shit
	mWorld->updateClientEcsData(mCameraCartesianDirection);

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

    // Update main thread update queues
    mWorld->updateTaskQueues();

    // Update game ticks
    int ticks = 0;
	while (mGameTimer.tryTick() && ticks++ < MAX_TICKS_PER_UPDATE) {

        // Update camera
        // TODO: Copy paste bad
        const PhysicsComponent& physCmp = mWorld->getECS().mRegistry.get<PhysicsComponent>(mPlayerEntity);
        const f32v2& playerXYPos = physCmp.getXYPosition();

		// World update after camera
        mWorld->update(playerXYPos, *mCamera3D);
	}
	// TODO: Actual usage of deltatime?
    updateCamera(gameTime);

	updateTilePicking();

}

void MainMenuScreen::draw(const vui::GameTime& gameTime)
{

	const f32 frameAlpha = mGameTimer.getFrameAlpha();

    // Grab fps
    sFps = vmath::lerp(sFps, m_app->getFps(), 0.85f);
    mFps = sFps;

    auto&& ecs = mWorld->getECS();
	PhysicsComponent& cmp = ecs.mRegistry.get<PhysicsComponent>(mPlayerEntity);
	const f32v2& xyPos = cmp.getXYPosition();
	mRenderContext.renderFrame(*mCamera3D, f32v3(xyPos.x, xyPos.y, cmp.getZPosition()), frameAlpha);

	tryUpdateAndRenderInteractPopup(xyPos);

	mRenderContext.endFrame();

}


void MainMenuScreen::updateCamera(const vui::GameTime& gameTime) {
	// Target player
    const f32 frameAlpha = mGameTimer.getFrameAlpha();
    const PhysicsComponent& physCmp = mWorld->getECS().mRegistry.get<PhysicsComponent>(mPlayerEntity);
    const f32v2& playerXYPos = physCmp.getXYInterpolated(frameAlpha);
    const f32 playerZPos = physCmp.getZInterpolated(frameAlpha);

    // TODO: Delta time dependent?
    // Zoom
	const PlayerControlComponent& playerControlCmp = mWorld->getECS().mRegistry.get<PlayerControlComponent>(mPlayerEntity);

    // Camera follow
    constexpr float MAX_SPEED_MPS = 0.3f;
    const f32 maxSpeed = MAX_SPEED_MPS * mCameraPositionTweener.mCurr.z;
	f32v3 targetPos(playerXYPos.x, playerXYPos.y, mCameraPositionTweener.mTarget.z);
	mCameraPositionTweener.setTarget(targetPos);
	mCameraPositionTweener.setMaxSpeed(MAX_SPEED_MPS * mCameraPositionTweener.mCurr.z);

	mCameraPositionTweener.update(1.0f);
	mCameraDirectionTweener.update(1.0f);

    const f32v3 lookAtOffset(mCameraDirectionTweener.mCurr.x, mCameraDirectionTweener.mCurr.y, mCameraDirectionZOffset);
    mCamera3D->lookAt(mCamera3D->getPosition() + lookAtOffset);

	mCamera3D->setPosition(mCameraPositionTweener.mCurr - lookAtOffset * mCameraPositionTweener.mCurr.z + f32v3(0.0f, 0.0f, playerZPos));

	if (mCamera3D->getFieldOfView() != sDebugOptions.mFoV) {
		mCamera3D->setFieldOfView(sDebugOptions.mFoV);
	}

	// Increase Z clip as camera goes higher to reduce precision issues and make fog move away from camera
	const f32 zNearAlpha = glm::clamp(mCamera3D->getPosition().z * 0.001f, 0.0f, 1.0f);
	const f32 zNear = lerp(0.1f, 5.0f, zNearAlpha);
	mCamera3D->setClippingPlane(zNear, sDebugOptions.mZFar);

	mCamera3D->update();

}

void MainMenuScreen::updateTilePicking() {
	const f32 normalizedX = (mMousePosition.x / (f32)m_app->getWindow().getWidth()) * 2.0f - 1.0f;
	const f32 normalizedY = -((mMousePosition.y / (f32)m_app->getWindow().getHeight()) * 2.0f - 1.0f);
	f32v4 pickRayClipSpace(normalizedX, normalizedY, -1.0f, 1.0f);
	f32v4 pickRayEyeSpace = glm::inverse(mCamera3D->getProjectionMatrix()) * pickRayClipSpace;
	pickRayEyeSpace.z = -1.0f;
	pickRayEyeSpace.w = 0.0f;
	f32v4 pickRayWorldSpace = glm::inverse(mCamera3D->getViewMatrix()) * pickRayEyeSpace;
	f32v3 pickRayXYZ(pickRayWorldSpace.x, pickRayWorldSpace.y, pickRayWorldSpace.z);
	sDebugOptions.mMousePickRay = glm::normalize(pickRayXYZ);
}


void MainMenuScreen::tryUpdateAndRenderInteractPopup(const f32v2& xyPos) {
    // Handle interact menu TODO: Notify to get this out of here
    if (mRightClickInteractPopup) {
        // Render selected
        const ui32v2 worldPosInt = mSelectedTilePosition;
        const f32 height = mWorld->getWorldGrid().tryComputeHeightAtPoint(f32v2(worldPosInt) + f32v2(0.5f));
        DebugRenderer::drawFilledQuad(f32v3(worldPosInt.x, worldPosInt.y, height), f32v2(1.0f), color4(1.0f, 1.0f, 0.0f, 0.5f));

        const UIInteractMenuResultFlags result = mRightClickInteractPopup->updateAndRender();
        // TODO: Notify
        if (result & INTERACT_MENU_RESULT_PATHFIND) {
            NavigationComponent& cmp = mWorld->getECS().mRegistry.get_or_emplace<NavigationComponent>(mPlayerEntity);
            cmp.requestCoarsePath(xyPos, worldPosInt);
        }
        else if (result & INTERACT_MENU_RESULT_CLEAR_TILE) {
            // grass
            TileHandle handle = mWorld->getTileHandleAtWorldPos(mSelectedTilePosition);
			// TODO: HANDLE RACE CONDITION
            if (handle.isValid()) {
                handle.getMutableChunk()->setTileAt(handle.index, Tile(TileRepository::getTile("grass1"), TILE_ID_NONE, TILE_ID_NONE));
            }
        }
        else if (result & INTERACT_MENU_RESULT_PLANT_TREE) {
            // grass
            TileHandle handle = mWorld->getTileHandleAtWorldPos(mSelectedTilePosition);
            if (handle.isValid()) {
                handle.getMutableChunk()->setTileAt(handle.index, Tile(TileRepository::getTile("grass1"), TILE_ID_NONE, TileRepository::getTile("tree_small")));
            }
        }
        else if (result & INTERACT_MENU_RESULT_PLANT_TREE_2) {
            // grass
            TileHandle handle = mWorld->getTileHandleAtWorldPos(mSelectedTilePosition);
            if (handle.isValid()) {
                handle.getMutableChunk()->setTileAt(handle.index, Tile(TileRepository::getTile("grass1"), TILE_ID_NONE, TileRepository::getTile("tree_pine")));
            }
        }
        else if (result & INTERACT_MENU_RESULT_BUILD_WALL) {
            // grass
            TileHandle handle = mWorld->getTileHandleAtWorldPos(mSelectedTilePosition);
            if (handle.isValid()) {
                handle.getMutableChunk()->setTileAt(handle.index, Tile(TileRepository::getTile("rock1"), TILE_ID_NONE, TILE_ID_NONE, 2u));
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
			woodPile.id = mResourceManager->getItemRepository().getItem("wood_raw").getID();
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

        }
        static_assert(INTERACT_MENU_RESULT_COUNT == 10, "update");

        // If we had a result, close window
        if (result) {
            mRightClickInteractPopup.reset();
            ImGui::GetIO().WantCaptureKeyboard = false;
            ImGui::GetIO().WantCaptureMouse = false;
        }
    }
}