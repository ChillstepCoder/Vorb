#include "stdafx.h"
#include "MainMenuScreen.h"

#include "App.h"

#include <Vorb/math/VorbMath.hpp>
#include <Vorb/ui/InputDispatcher.h>
#include <Vorb/graphics/SpriteFont.h>
#include <Vorb/graphics/TextureCache.h>
#include <glm/gtx/rotate_vector.hpp>

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

// size of global cached random table
const unsigned CACHED_RANDOM_SIZE = 65536;

MainMenuScreen::MainMenuScreen(const App* app) 
	: IAppScreen<App>(app),
	  mResourceManager(std::make_unique<ResourceManager>()),
	  mWorld(std::make_unique<World>(*mResourceManager)),
      mRenderContext(RenderContext::initInstance(*mResourceManager, *mWorld, f32v2(m_app->getWindow().getWidth(), m_app->getWindow().getHeight())))
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

	Random::initCachedRandom(CACHED_RANDOM_SIZE);

	mCircleTexture = mResourceManager->getTextureCache().addTexture("data/textures/circle_dir.png");

	const f32v2 screenSize(m_app->getWindow().getWidth(), m_app->getWindow().getHeight());
	mCamera2D->init((int)screenSize.x, (int)screenSize.y);
	mCamera2D->setScale(mScale);

    mResourceManager->gatherFiles("data");
	mResourceManager->loadFiles();

    mRenderContext.initPostLoad();
    mResourceManager->writeDebugAtlas();

	vui::InputDispatcher::key.onKeyDown.addFunctor([this](Sender sender, const vui::KeyEvent& event) {
		// View toggle
		if (event.keyCode == VKEY_B) {
			sDebugOptions.mWireframe = !sDebugOptions.mWireframe;
        } else if (event.keyCode == VKEY_C) {
            sDebugOptions.mChunkBoundaries = !sDebugOptions.mChunkBoundaries;
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
		mTargetScale = glm::clamp(mTargetScale + event.dy * mTargetScale * 0.2f, 0.09f, 1020.f);
	});

	vui::InputDispatcher::mouse.onButtonDown.addFunctor([this](Sender sender, const vui::MouseButtonEvent& event) {
		mTestClick = mCamera2D->convertScreenToWorld(f32v2(event.x, event.y));

		// Set tiles
		//int tileIndex = m_tileGrid->getTileIndexFromScreenPos(m_testClick, *m_camera2D);
		//m_tileGrid->setTile(tileIndex, TileGrid::STONE_1);
	});

	vui::InputDispatcher::mouse.onButtonUp.addFunctor([this](Sender sender, const vui::MouseButtonEvent& event) {
		constexpr float VEL_MULT = 0.0001f;
		constexpr float VEL_EXP = 0.4f;
		const f32v2 worldPos = mCamera2D->convertScreenToWorld(f32v2(event.x, event.y));
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
			else {
				TileHandle handle = mWorld->getTileHandleAtWorldPos(worldPos);
				if (handle.isValid()) {
					handle.getMutableChunk()->setTileAt(handle.index, Tile(TileRepository::getTile("rock1"), TILE_ID_NONE, TILE_ID_NONE));
				}
			}
        }
        else if (event.button == vui::MouseButton::RIGHT) {
            /*newActor = mHumanActorFactory->createActor(
                mTestClick,
                vio::Path("data/textures/circle_dir.png"),
                vio::Pathw("")
            );*/
			const f32v3 pos(worldPos.x, worldPos.y, 0.5f);
			mResourceManager->getParticleSystemManager().createParticleSystem(pos, f32v3(1.0f, 0.0f, 0.0f), "blood");
		}

		// Apply velocity
		if (newActor != INVALID_ENTITY) {
            /*auto& physcomp = mecs->getphysicscomponentfromentity(newactor);
            velocity = velocity;
            physcomp.mbody->applyforce(reinterpret_cast<b2vec2&>(velocity), physcomp.mbody->getworldcenter(), true);*/
		}
	});

	// Add player
	mPlayerEntity = mWorld->createEntity(WorldData::WORLD_CENTER, EntityType::PLAYER);
	mCamera2D->setPosition(WorldData::WORLD_CENTER);

}

void MainMenuScreen::destroy(const vui::GameTime& gameTime) {
	
}

void MainMenuScreen::onEntry(const vui::GameTime& gameTime) {
}

void MainMenuScreen::onExit(const vui::GameTime& gameTime) {
}

void MainMenuScreen::update(const vui::GameTime& gameTime) {

	const float deltaTime = /*gameTime.elapsed / (1.0f / 60.0f)*/ 1.0f;

	// Do this first
	mWorld->updateClientEcsData(*mCamera2D);

	// DEBUG Time advance
	static constexpr float TIME_ADVANCE_MULT = 100.0f;
    if (vui::InputDispatcher::key.isKeyPressed(VKEY_LEFT)) {
        sDebugOptions.mTimeOffset -= gameTime.elapsedSec * TIME_ADVANCE_MULT;
    }
    else if (vui::InputDispatcher::key.isKeyPressed(VKEY_RIGHT)) {
        sDebugOptions.mTimeOffset += gameTime.elapsedSec * TIME_ADVANCE_MULT;
    }

    const f32v2& playerPos = mWorld->getECS().mRegistry.get<PhysicsComponent>(mPlayerEntity).getPosition();

	updateCamera(playerPos, gameTime);

	mWorld->update(deltaTime, playerPos, *mCamera2D);

	// Update
	sFps = vmath::lerp(sFps, m_app->getFps(), 0.85f);
	mFps = sFps;
}

void MainMenuScreen::draw(const vui::GameTime& gameTime)
{

	mRenderContext.renderFrame(*mCamera2D);


    /*mSb->begin();
    char fpsString[64];
    sprintf_s(fpsString, sizeof(fpsString), "FPS %d", (int)std::round(mFps));
    mSb->drawString(mSpriteFont.get(), fpsString, f32v2(0.0f, mCamera2D->getScreenHeight() - 32.0f), f32v2(1.0f, 1.0f), color4(1.0f, 1.0f, 1.0f));
    mSb->end();
    mSb->render(mCamera2D->getScreenSize());*/
	
}

void MainMenuScreen::updateCamera(const f32v2& targetCenter, const vui::GameTime& gameTime) {

    // TODO: Delta time dependent?
    // Zoom
    if (abs(mTargetScale - mScale) > 0.001f) {
        mScale = vmath::lerp(mScale, mTargetScale, 0.3f);
        mCamera2D->setScale(mScale);
    }
	const float cameraHeight = 1.0f / mScale * 1000.0f; // Height in meters

    const f32v2& currentPos = mCamera2D->getPosition();

    // Camera follow
    const f32v2& offsetToMouse = mWorld->getClientECSData().worldMousePos - currentPos;
    constexpr float LOOK_SCALE = 0.4f;
    mTargetCameraPosition = targetCenter + offsetToMouse * LOOK_SCALE;

	const f32v2 offsetToTarget = mTargetCameraPosition - currentPos;
	const float distanceToTarget = glm::length(offsetToTarget);
	const f32v2 normalToTarget = offsetToTarget / distanceToTarget;

	constexpr float MAX_SPEED_MPS = 0.1f;
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
