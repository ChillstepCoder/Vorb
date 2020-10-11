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

#include "physics/ContactListener.h"
#include "rendering/RenderContext.h"

#include "TextureManip.h"

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

	mCircleTexture = mResourceManager->getTextureCache().addTexture("data/textures/circle_dir.png");

	const f32v2 screenSize(m_app->getWindow().getWidth(), m_app->getWindow().getHeight());
	mCamera2D->init((int)screenSize.x, (int)screenSize.y);
	mCamera2D->setScale(mScale);

    mResourceManager->gatherFiles("data");
	mResourceManager->loadFiles();
	mResourceManager->writeDebugAtlas();

	mRenderContext.initPostLoad();

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
	});

	vui::InputDispatcher::mouse.onWheel.addFunctor([this](Sender sender, const vui::MouseWheelEvent& event) {
		mScale = glm::clamp(mScale + event.dy * mScale * 0.2f, 1.0f, 1020.f);
		mCamera2D->setScale(mScale);
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

		vecs::EntityID newActor = 0;
		if (event.button == vui::MouseButton::LEFT) {
            /*newActor = mUndeadActorFactory->createActor(
                mTestClick,
                vio::Path("data/textures/circle_dir.png"),
                vio::Path("")
            );*/
			TileHandle handle = mWorld->getTileHandleAtWorldPos(worldPos);
			if (handle.isValid()) {
				handle.getMutableChunk()->setTileAt(handle.index, Tile(TileRepository::getTile("rock1"), TILE_ID_NONE, TILE_ID_NONE));
			}
		}
		else if (event.button == vui::MouseButton::RIGHT) {
            /*newActor = mHumanActorFactory->createActor(
                mTestClick,
                vio::Path("data/textures/circle_dir.png"),
                vio::Path("")
            );*/
            TileHandle handle = mWorld->getTileHandleAtWorldPos(worldPos);
			if (handle.isValid()) {
				handle.getMutableChunk()->setTileAt(handle.index, Tile(TileRepository::getTile("grass1"), TILE_ID_NONE, TILE_ID_NONE));
			}
		}

		// Apply velocity
		if (newActor) {
            /*auto& physcomp = mecs->getphysicscomponentfromentity(newactor);
            velocity = velocity;
            physcomp.mbody->applyforce(reinterpret_cast<b2vec2&>(velocity), physcomp.mbody->getworldcenter(), true);*/
		}
	});

	// Add player
	mPlayerEntity = mWorld->createEntity(f32v2(0.0f), EntityType::PLAYER);

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

	//static const f32v2 CAM_VELOCITY(5.0f, 5.0f);
	//f32v2 offset(0.0f);

	// Camera movement
	/*if (vui::InputDispatcher::key.isKeyPressed(VKEY_LEFT) || vui::InputDispatcher::key.isKeyPressed(VKEY_A)) {
		offset.x -= CAM_VELOCITY.x * deltaTime;
	}
	else if (vui::InputDispatcher::key.isKeyPressed(VKEY_RIGHT) || vui::InputDispatcher::key.isKeyPressed(VKEY_D)) {
		offset.x += CAM_VELOCITY.x * deltaTime;
	}

	if (vui::InputDispatcher::key.isKeyPressed(VKEY_UP) || vui::InputDispatcher::key.isKeyPressed(VKEY_W)) {
		offset.y += CAM_VELOCITY.x * deltaTime;
	}
	else if (vui::InputDispatcher::key.isKeyPressed(VKEY_DOWN) || vui::InputDispatcher::key.isKeyPressed(VKEY_S)) {
		offset.y -= CAM_VELOCITY.x * deltaTime;
	}

	if (offset.x != 0.0f || offset.y != 0.0f) {
		mCamera2D->offsetPosition(offset);
	}*/

	// Camera follow


    const f32v2& playerPos = mWorld->getECS().getPhysicsComponentFromEntity(mPlayerEntity).getPosition();
	const f32v2& offset = mWorld->getClientECSData().worldMousePos - playerPos;
	mCamera2D->setPosition(playerPos + offset * 0.2f);
	mCamera2D->update();

	mWorld->update(deltaTime, playerPos, *mCamera2D);

	// Update
	mFps = vmath::lerp(mFps, m_app->getFps(), 0.85f);
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