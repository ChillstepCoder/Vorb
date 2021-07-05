#include "stdafx.h"
#include "MainMenuScreen.h"

#include "App.h"

#include <Vorb/math/VorbMath.hpp>
#include <Vorb/ui/InputDispatcher.h>
#include <Vorb/graphics/SpriteFont.h>
#include <Vorb/graphics/TextureCache.h>
#include <glm/gtx/rotate_vector.hpp>

#include "DebugRenderer.h"

#include <box2d/b2_body.h>
#include <box2d/b2_contact.h>

#include "Camera2D.h"

#include "World.h"
#include "Utils.h"

#include "ResourceManager.h"
#include "particles/ParticleSystemManager.h"

#include "physics/ContactListener.h"
#include "rendering/RenderContext.h"

#include "TextureManip.h"
#include "Random.h"

#include "ui/UIInteractMenuPopup.h"

#include <SDL.h>

#include <Vorb/ui/imgui/imgui.h>

constexpr ui32 MAX_TICKS_PER_UPDATE = 2;
constexpr f64 TICK_RATE_MS = 40.0;

MainMenuScreen::MainMenuScreen(const App* app) 
	: IAppScreen<App>(app),
	  mResourceManager(&Services::ResourceManager::ref()),
      mRenderContext(RenderContext::initInstance(*mResourceManager, *mWorld, f32v2(m_app->getWindow().getWidth(), m_app->getWindow().getHeight()))),
      mWorld(std::make_unique<World>(*mResourceManager))
{

	mCamera2D = std::make_unique<Camera2D>();

    // TODO: This is kinda stupid
    if (WeaponRegistry::s_allWeaponItems.empty()) {
        ArmorRegistry::loadArmors();
        WeaponRegistry::loadWeapons();
        ShieldRegistry::loadShields();
    }

	// Starting time of day to noon
	mWorld->setTimeOfDay(12.0f);


	// TODO: A battle is just a graph, with connections between units who are engaging. When engaging units do not need to do any area
	// checks. When initiating combat, area checks can be stopped. Units simply check the graph and do AI based on what is around them.
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
	mCamera2D->init((int)screenSize.x, (int)screenSize.y);
	mCamera2D->setScale(mScale);

    mResourceManager->gatherFiles("data");
	mResourceManager->loadFiles();

    mRenderContext.initPostLoad();
    mResourceManager->writeDebugAtlas();

	mWorld->initPostLoad();

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
        else if (event.keyCode == VKEY_R) {
			mRenderContext.reloadShaders();
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
	});

	vui::InputDispatcher::mouse.onWheel.addFunctor([this](Sender sender, const vui::MouseWheelEvent& event) {
		mTargetScale = glm::clamp(mTargetScale + event.dy * mTargetScale * 0.2f, 0.03f, 1020.f);
	});

	vui::InputDispatcher::mouse.onButtonDown.addFunctor([this](Sender sender, const vui::MouseButtonEvent& event) {
		const f32v2 screenPos(event.x, event.y);
		mTestClick = mCamera2D->convertScreenToWorld(screenPos);

		if (event.button == vui::MouseButton::RIGHT) {
			// If we are making a villager with G, dont freeze screen
			if (!vui::InputDispatcher::key.isKeyPressed(VKEY_G)) {
				mIsRightButtonDown = true;
				mLastRightClickPosition = screenPos;
			}
		}

		// Set tiles
		//int tileIndex = m_tileGrid->getTileIndexFromScreenPos(m_testClick, *m_camera2D);
		//m_tileGrid->setTile(tileIndex, TileGrid::STONE_1);
	});

	vui::InputDispatcher::mouse.onButtonUp.addFunctor([this](Sender sender, const vui::MouseButtonEvent& event) {
		constexpr float VEL_MULT = 0.0001f;
		constexpr float VEL_EXP = 0.4f;
		const f32v2 screenPos(event.x, event.y);
		const f32v2 worldPos = mCamera2D->convertScreenToWorld(screenPos);
		const f32v2 offset = worldPos - mTestClick;
		const float mag = glm::length(offset);
		const float power = pow(mag * VEL_MULT, VEL_EXP);
		f32v2 velocity;
		if (mag == 0.0f) {
			velocity = f32v2(0.0f);
		}
		else {
			velocity = (offset / mag) * power;
		}

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
					phys->teleportToPoint(worldPos);
				}
			}
			else if (vui::InputDispatcher::key.isKeyPressed(VKEY_Q)) {
                TileHandle handle = mWorld->getTileHandleAtWorldPos(worldPos);
                if (handle.isValid()) {
					Chunk* chunk = handle.getMutableChunk();
					ui16 height = chunk->getTileAt(handle.index).baseZPosition + 5;
					chunk->setTileAt(handle.index, Tile(TileRepository::getTile("rock1"), TILE_ID_NONE, TILE_ID_NONE, height));
                }
			}
            else if (vui::InputDispatcher::key.isKeyPressed(VKEY_E)) {
                TileHandle handle = mWorld->getTileHandleAtWorldPos(worldPos);
                if (handle.isValid()) {
                    Chunk* chunk = handle.getMutableChunk();
                    ui16 height = chunk->getTileAt(handle.index).baseZPosition;
                    chunk->setTileAt(handle.index, Tile(TileRepository::getTile("rock1"), TILE_ID_NONE, TILE_ID_NONE, height));
                }
            }
            else if (vui::InputDispatcher::key.isKeyPressed(VKEY_C)) {
                TileHandle handle = mWorld->getTileHandleAtWorldPos(worldPos);
				mWorld->createCityAt(ui32v2(floor(worldPos.x), floor(worldPos.y)));
            }
			else {
				if (mRightClickInteractPopup) {
					mRightClickInteractPopup.reset();
				}
			}
        }
        else if (event.button == vui::MouseButton::RIGHT) {
            mIsRightButtonDown = false;
			if (vui::InputDispatcher::key.isKeyPressed(VKEY_P)) {
                const f32v3 pos(worldPos.x, worldPos.y, 0.5f);
                mResourceManager->getParticleSystemManager().createParticleSystem(pos, f32v3(1.0f, 0.0f, 0.0f), "blood");
			}
			else if (vui::InputDispatcher::key.isKeyPressed(VKEY_G)) {
                mWorld->createEntity(worldPos, "villager");
			}
            else {
                if (mRightClickInteractPopup) {
                    mRightClickInteractPopup.reset();
					SDL_WarpMouseInWindow(static_cast<SDL_Window*>(m_app->getWindow().getHandle()), mLastRightClickPosition.x, mLastRightClickPosition.y);
				}
				else {
					// Right click picking
					mSelectedTilePosition = worldPos;
					// Enable context menu
					mRightClickInteractPopup = std::make_unique<UIInteractMenuPopup>(screenPos, static_cast<SDL_Window*>(m_app->getWindow().getHandle()));
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
	mCamera2D->setPosition(WorldData::WORLD_CENTER);

}

void MainMenuScreen::destroy(const vui::GameTime& gameTime) {
	
}

void MainMenuScreen::onEntry(const vui::GameTime& gameTime) {
}

void MainMenuScreen::onExit(const vui::GameTime& gameTime) {
}

void MainMenuScreen::update(const vui::GameTime& gameTime) {

	mGameTimer.startFrame();

    bool didUpdateCamera = false;

    // Store mouse position and other useful things
	mWorld->updateClientEcsData(*mCamera2D);

	while (mGameTimer.tryTick()) {

		// DEBUG Time advance
		static constexpr float TIME_ADVANCE_MULT = 4.0f;
		if (vui::InputDispatcher::key.isKeyPressed(VKEY_LEFT)) {
			if (vui::InputDispatcher::key.isKeyPressed(VKEY_LSHIFT)) {
                sDebugOptions.mTimeOffset -= gameTime.elapsedSec * 250.0f;
			}
			else {
				sDebugOptions.mTimeOffset -= gameTime.elapsedSec * TIME_ADVANCE_MULT;
			}
		}
        else if (vui::InputDispatcher::key.isKeyPressed(VKEY_RIGHT)) {
            if (vui::InputDispatcher::key.isKeyPressed(VKEY_LSHIFT)) {
                sDebugOptions.mTimeOffset += gameTime.elapsedSec * 250.0f;
            }
			else {
				sDebugOptions.mTimeOffset += gameTime.elapsedSec * TIME_ADVANCE_MULT;
			}
			mGameTimer.setMsPerTick(MS_PER_GAME_TICK / TIME_ADVANCE_MULT);
		}
		else {
            mGameTimer.setMsPerTick(MS_PER_GAME_TICK);
		}

        // Update camera
        // TODO: Copy paste bad
        const PhysicsComponent& physCmp = mWorld->getECS().mRegistry.get<PhysicsComponent>(mPlayerEntity);
        const f32v2& playerXYPos = physCmp.getXYPosition();
		if (!mIsRightButtonDown && !mRightClickInteractPopup) {
			f32v2 targetPos = playerXYPos;
			targetPos.y += physCmp.getZPosition() * Z_TO_XY_RATIO;
			updateCamera(targetPos, physCmp.getZPosition(), gameTime);
		}
		didUpdateCamera = true;

		// World update after camera
        mWorld->update(playerXYPos, *mCamera2D);
	}
	// Always update camera one last time using interpolated position
	// TODO: Copy paste bad
	const f32 frameAlpha = mGameTimer.getFrameAlpha();
	const PhysicsComponent& physCmp = mWorld->getECS().mRegistry.get<PhysicsComponent>(mPlayerEntity);
    const f32v2& playerXYPos = physCmp.getXYInterpolated(frameAlpha);
	if (!mIsRightButtonDown && !mRightClickInteractPopup) {
		f32v2 targetPos = playerXYPos;
		targetPos.y += physCmp.getZInterpolated(frameAlpha) * Z_TO_XY_RATIO;
		updateCamera(targetPos, physCmp.getZInterpolated(frameAlpha), gameTime);
	}
    didUpdateCamera = true;

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
	mRenderContext.renderFrame(*mCamera2D, f32v3(xyPos.x, xyPos.y, cmp.getZPosition()), mWorld->getClientECSData().worldMousePos, frameAlpha);

	// Draw selection drag
	if (mIsRightButtonDown) {
		const ui32v2 tilePos = mWorld->getClientECSData().worldMousePos;
		DebugRenderer::drawQuad(tilePos, f32v2(1.0f), color4(0.0f, 1.0f, 0.0f, 0.5f));
	}

	// Handle interact menu TODO: Notify to get this out of here
	if (mRightClickInteractPopup) {
        // Render selected
        ui32v2 worldPosInt = mSelectedTilePosition;
		DebugRenderer::drawQuad(worldPosInt, f32v2(1.0f), color4(1.0f, 1.0f, 0.0f, 0.5f));

		// Draw vectors to corners
		f32v2 interactPopupPositionWorld = mCamera2D->convertScreenToWorld(mLastRightClickPosition);
		const color4 lineColor = color4(1.0f, 1.0f, 0.0f, 1.0f);
		DebugRenderer::drawLineBetweenPoints(mSelectedTilePosition, interactPopupPositionWorld, lineColor);
		
		const UIInteractMenuResultFlags result = mRightClickInteractPopup->updateAndRender();
		// TODO: Notify
		if (result & INTERACT_MENU_RESULT_PATHFIND) {
			NavigationComponent& cmp = mWorld->getECS().mRegistry.get_or_emplace<NavigationComponent>(mPlayerEntity);
			cmp.mPath = Services::PathFinder::ref().generatePathSynchronous(*mWorld, worldPosInt, xyPos);
			cmp.mCurrentPoint = 0;
			DebugRenderer::drawPath(*cmp.mPath, color4(1.0f, 0.0f, 1.0f), 200);
		}
		else if (result & INTERACT_MENU_RESULT_CLEAR_TILE) {
            // grass
            TileHandle handle = mWorld->getTileHandleAtWorldPos(mSelectedTilePosition);
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
        else if (result & INTERACT_MENU_RESULT_BUILD_WALL) {
            // grass
            TileHandle handle = mWorld->getTileHandleAtWorldPos(mSelectedTilePosition);
            if (handle.isValid()) {
                handle.getMutableChunk()->setTileAt(handle.index, Tile(TileRepository::getTile("rock1"), TILE_ID_NONE, TILE_ID_NONE));
            }
        }
        static_assert(INTERACT_MENU_RESULT_COUNT == 5, "update");

		// If we had a result, close window
		if (result) {
			mRightClickInteractPopup.reset();
            ImGui::GetIO().WantCaptureKeyboard = false;
            ImGui::GetIO().WantCaptureMouse = false;
			
			// Warp mouse
			SDL_WarpMouseInWindow(static_cast<SDL_Window*>(m_app->getWindow().getHandle()), mLastRightClickPosition.x, mLastRightClickPosition.y);
		}
	} 

    /*mSb->begin();
    char fpsString[64];
    sprintf_s(fpsString, sizeof(fpsString), "FPS %d", (int)std::round(mFps));
    mSb->drawString(mSpriteFont.get(), fpsString, f32v2(0.0f, mCamera2D->getScreenHeight() - 32.0f), f32v2(1.0f, 1.0f), color4(1.0f, 1.0f, 1.0f));
    mSb->end();
    mSb->render(mCamera2D->getScreenSize());*/
	
}

void MainMenuScreen::updateCamera(const f32v2& targetCenter, f32 targetHeight, const vui::GameTime& gameTime) {
	UNUSED(targetHeight);
	// TODO: use targetHeight to affect zoom
    // TODO: Delta time dependent?
    // Zoom
	const PlayerControlComponent& playerControlCmp = mWorld->getECS().mRegistry.get<PlayerControlComponent>(mPlayerEntity);
    if (abs(mTargetScale - mScale) > 0.001f) {
        mScale = vmath::lerp(mScale, mTargetScale, 0.3f);
        mCamera2D->setScale(mScale);
    }
	const float cameraHeight = 1.0f / mScale * 1000.0f; // Height in meters

    const f32v2& currentPos = mCamera2D->getPosition();

    // Camera follow
    const f32v2& offsetToMouse = mWorld->getClientECSData().worldMousePos - currentPos;
    constexpr float LOOK_SCALE = 1.0f;
	mTargetCameraPosition = targetCenter + offsetToMouse * LOOK_SCALE;

	const f32v2 offsetToTarget = mTargetCameraPosition - currentPos;
	const float distanceToTarget = glm::length(offsetToTarget);
	const f32v2 normalToTarget = offsetToTarget / distanceToTarget;

	constexpr float MAX_SPEED_MPS = 0.15f;
	const f32 maxSpeed = MAX_SPEED_MPS * cameraHeight;
    f32v2 maxTargetVelocity = normalToTarget * maxSpeed;

    // How fast are we going in the correct direction?
    const f32v2 projectedVelocity = (glm::dot(mCameraVelocity, maxTargetVelocity) / glm::length2(maxTargetVelocity)) * maxTargetVelocity;
	const f32 projectedSpeed = glm::length(projectedVelocity);
    
    bool isDecelerating = false;
    if (distanceToTarget < maxSpeed * 10.0f) {
        maxTargetVelocity *= (distanceToTarget / (maxSpeed * 10.0f));
        if (projectedSpeed >= glm::length(maxTargetVelocity)) {
            isDecelerating = true;
        }
	}

	// Smooth accelerate, abrupt decelerate
	if (isDecelerating) {
		mCameraVelocity = vmath::lerp(mCameraVelocity, maxTargetVelocity, 0.9f);
	}
	else {
		mCameraVelocity = vmath::lerp(mCameraVelocity, maxTargetVelocity, 0.1f);
    }

    mCamera2D->setPosition(currentPos + mCameraVelocity);

    mCamera2D->update();
}
