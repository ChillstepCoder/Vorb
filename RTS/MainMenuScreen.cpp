#include "stdafx.h"
#include "MainMenuScreen.h"

#include "App.h"

#include <Vorb/io/IOManager.h>
#include <Vorb/math/VorbMath.hpp>
#include <Vorb/graphics/SpriteBatch.h>
#include <Vorb/graphics/TextureCache.h>
#include <Vorb/graphics/DepthState.h>
#include <Vorb/ui/InputDispatcher.h>
#include <Vorb/graphics/SpriteFont.h>
#include <glm/gtx/rotate_vector.hpp>

#include <box2d/b2_body.h>
#include <box2d/b2_contact.h>

#include "Camera2D.h"

#include "TileGrid.h"
#include "Utils.h"

#include "actor/HumanActorFactory.h"
#include "actor/UndeadActorFactory.h"
#include "actor/PlayerActorFactory.h"

#include "physics/ContactListener.h"

#include "DebugRenderer.h"

MainMenuScreen::MainMenuScreen(const App* app) 
	: IAppScreen<App>(app)
	, mPhysWorld(b2Vec2(0.0f, 0.0f))
	, mEcs(mPhysWorld){

	// TODO: This is kinda stupid
	if (WeaponRegistry::s_allWeaponItems.empty()) {
		ArmorRegistry::loadArmors();
		WeaponRegistry::loadWeapons();
		ShieldRegistry::loadShields();
	}

	mSb = std::make_unique<vg::SpriteBatch>();
	mTextureCache = std::make_unique<vg::TextureCache>();

	mIoManager = std::make_unique<vio::IOManager>();

	mCamera2D = std::make_unique<Camera2D>();

	mTileGrid = std::make_unique<TileGrid>(mPhysWorld, i32v2(50, 50), *mTextureCache, mEcs, "data/textures/tiles.png", i32v2(10, 16), 60.0f);

	mEcsRenderer = std::make_unique<EntityComponentSystemRenderer>(*mTextureCache, mEcs, *mTileGrid);
	mSpriteFont = std::make_unique<vg::SpriteFont>();

	mHumanActorFactory = std::make_unique<HumanActorFactory>(mEcs, *mTextureCache);
	mUndeadActorFactory = std::make_unique<UndeadActorFactory>(mEcs, *mTextureCache);
	mPlayerActorFactory = std::make_unique<PlayerActorFactory>(mEcs, *mTextureCache);

	mContactListener = std::make_unique<ContactListener>(mEcs);
	mPhysWorld.SetContactListener(mContactListener.get());

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
	mSb->init();
	mTextureCache->init(mIoManager.get());
	mSpriteFont->init("data/fonts/chintzy.ttf", 32);

	mCircleTexture = mTextureCache->addTexture("data/textures/circle_dir.png");

	const f32v2 screenSize(m_app->getWindow().getWidth(), m_app->getWindow().getHeight());
	mCamera2D->init((int)screenSize.x, (int)screenSize.y);
	mCamera2D->setScale(mScale);

	vui::InputDispatcher::key.onKeyDown.addFunctor([this](Sender sender, const vui::KeyEvent& event) {
		// View toggle
		if (event.keyCode == VKEY_V) {
			if (mTileGrid->getView() == TileGrid::View::ISO) {
				mTileGrid->setView(TileGrid::View::TOP_DOWN);
			}
			else {
				mTileGrid->setView(TileGrid::View::ISO);
			}
		}
	});

	vui::InputDispatcher::mouse.onWheel.addFunctor([this](Sender sender, const vui::MouseWheelEvent& event) {
		mScale = glm::clamp(mScale + event.dy * 1.0f, 10.0f, 50.f);
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
		const f32v2 offset = mCamera2D->convertScreenToWorld(f32v2(event.x, event.y)) - mTestClick;
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
			newActor = mUndeadActorFactory->createActor(
				mTileGrid->convertScreenCoordToWorld(mTestClick),
				vio::Path("data/textures/circle_dir.png"),
				vio::Path("")
			);
		}
		else if (event.button == vui::MouseButton::RIGHT) {
			newActor = mHumanActorFactory->createActor(
				mTileGrid->convertScreenCoordToWorld(mTestClick),
				vio::Path("data/textures/circle_dir.png"),
				vio::Path("")
			);
		}

		// Apply velocity
		auto& physComp = mEcs.getPhysicsComponentFromEntity(newActor);
		velocity = mTileGrid->convertScreenCoordToWorld(velocity);
		physComp.mBody->ApplyForce(reinterpret_cast<b2Vec2&>(velocity), physComp.mBody->GetWorldCenter(), true);
	});


	// Add player
	mPlayerEntity = mPlayerActorFactory->createActor(mTileGrid->convertScreenCoordToWorld(f32v2(0.0f)),
		vio::Path("data/textures/circle_dir.png"),
		vio::Path(""));
}

void MainMenuScreen::destroy(const vui::GameTime& gameTime) {
	mSb->dispose();
	mTextureCache->dispose();
}

void MainMenuScreen::onEntry(const vui::GameTime& gameTime) {
}

void MainMenuScreen::onExit(const vui::GameTime& gameTime) {
}

void MainMenuScreen::update(const vui::GameTime& gameTime) {

	const float deltaTime = /*gameTime.elapsed / (1.0f / 60.0f)*/ 1.0f;
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

	f32v2 cameraFollow = mEcs.getPhysicsComponentFromEntity(mPlayerEntity).getPosition();
	cameraFollow = mTileGrid->convertWorldCoordToScreen(cameraFollow);
	mCamera2D->setPosition(cameraFollow);

	mCamera2D->update();
	mTileGrid->updateWorldMousePos(*mCamera2D.get());

	mEcs.update(deltaTime, *mTileGrid);

	// Update
	mFps = vmath::lerp(mFps, m_app->getFps(), 0.85f);
}

void MainMenuScreen::draw(const vui::GameTime& gameTime)
{
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	mTileGrid->draw(*mCamera2D);


	DebugRenderer::drawVector(f32v2(0.0f), mTileGrid->mAxis[0] * 5.0f, color4(1.0f, 0.0f, 0.0f));
	DebugRenderer::drawVector(f32v2(0.0f), -mTileGrid->mAxis[1] * 5.0f, color4(0.0f, 1.0f, 0.0f));
	DebugRenderer::drawVector(f32v2(0.0f), f32v2(0.0f, 1.0f) * 4.0f, color4(0.0f, 0.0f, 1.0f));

	//mEcsRenderer->renderPhysicsDebug(*m_camera2D);
	mEcsRenderer->renderSimpleSprites(*mCamera2D);
	mEcsRenderer->renderCharacterModels(*mCamera2D);

	if (vui::InputDispatcher::mouse.isButtonPressed(vui::MouseButton::LEFT)) {
		mSb->draw(mCircleTexture.id, mTestClick - f32v2(8.0f), f32v2(16.0f), color4(1.0f, 0.0f, 1.0f));
		const f32v2 pos = mCamera2D->convertScreenToWorld(vui::InputDispatcher::mouse.getPosition());
		DebugRenderer::drawVector(mTestClick, pos - mTestClick, color4(1.0f, 0.0f, 0.0f));
	}

	DebugRenderer::renderLines(mCamera2D->getCameraMatrix());

	mSb->begin();
	char fpsString[64];
	sprintf_s(fpsString, sizeof(fpsString), "FPS %d", (int)std::round(mFps));
	mSb->drawString(mSpriteFont.get(), fpsString, f32v2(0.0f, mCamera2D->getScreenHeight() - 32.0f), f32v2(1.0f, 1.0f), color4(1.0f, 1.0f, 1.0f));
	mSb->end();
	mSb->render(mCamera2D->getScreenSize());

	
}
