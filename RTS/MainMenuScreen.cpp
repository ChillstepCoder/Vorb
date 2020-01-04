#include "stdafx.h"
#include "MainMenuScreen.h"

#include "App.h"

#include <Vorb/io/IOManager.h>
#include <Vorb/math/VorbMath.hpp>
#include <Vorb/graphics/SpriteBatch.h>
#include <Vorb/graphics/TextureCache.h>
#include <Vorb/ui/InputDispatcher.h>
#include <Vorb/graphics/SpriteFont.h>
#include <glm/gtx/rotate_vector.hpp>

#include "Camera2D.h"

#include "TileGrid.h"
#include "Utils.h"

#include "actor/UndeadActorFactory.h"

#include "DebugRenderer.h"

MainMenuScreen::MainMenuScreen(const App* app) 
	: IAppScreen<App>(app) {
	mSb = std::make_unique<vg::SpriteBatch>();
	mTextureCache = std::make_unique<vg::TextureCache>();

	mIoManager = std::make_unique<vio::IOManager>();

	mCamera2D = std::make_unique<Camera2D>();

	mTileGrid = std::make_unique<TileGrid>(i32v2(50, 50), *mTextureCache, "data/textures/tiles.png", i32v2(10, 16), 60.0f);

	mEcsRenderer = std::make_unique<EntityComponentSystemRenderer>(*mTextureCache, mEcs, *mTileGrid);
	mSpriteFont = std::make_unique<vg::SpriteFont>();

	mUndeadActorFactory = std::make_unique<UndeadActorFactory>(mEcs, *mTextureCache);
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

	mCircleTexture = mTextureCache->addTexture("data/textures/circle.png");

	const f32v2 screenSize(m_app->getWindow().getWidth(), m_app->getWindow().getHeight());
	mCamera2D->init((int)screenSize.x, (int)screenSize.y);

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
		mScale = glm::clamp(mScale + event.dy * 0.05f, 0.5f, 1.5f);
		mCamera2D->setScale(mScale);
	});

	vui::InputDispatcher::mouse.onButtonDown.addFunctor([this](Sender sender, const vui::MouseButtonEvent& event) {
		mTestClick = mCamera2D->convertScreenToWorld(f32v2(event.x, event.y));

		// Set tiles
		//int tileIndex = m_tileGrid->getTileIndexFromScreenPos(m_testClick, *m_camera2D);
		//m_tileGrid->setTile(tileIndex, TileGrid::STONE_1);
	});

	vui::InputDispatcher::mouse.onButtonUp.addFunctor([this](Sender sender, const vui::MouseButtonEvent& event) {
		constexpr float VEL_MULT = 0.5f;
		constexpr float VEL_EXP = 0.5f;
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
		vecs::EntityID newActor = mUndeadActorFactory->createActorWithVelocity(
			mTileGrid->convertScreenCoordToWorld(mTestClick),
			mTileGrid->convertScreenCoordToWorld(velocity),
			vio::Path("data/textures/circle.png"),
			vio::Path("")
		);
	});
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

	const float deltaTime = gameTime.elapsed / (1.0f / 60.0f);
	static const f32v2 CAM_VELOCITY(5.0f, 5.0f);
	f32v2 offset(0.0f);

	if (vui::InputDispatcher::key.isKeyPressed(VKEY_LEFT) || vui::InputDispatcher::key.isKeyPressed(VKEY_A)) {
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
	}

	mCamera2D->update();

	mEcs.update(deltaTime);

	// Update
	mFps = vmath::lerp(mFps, m_app->getFps(), 0.85f);
}

void MainMenuScreen::draw(const vui::GameTime& gameTime)
{
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	mTileGrid->draw(*mCamera2D);


	DebugRenderer::drawVector(f32v2(0.0f), mTileGrid->mAxis[0] * 170.0f, color4(1.0f, 0.0f, 0.0f));
	DebugRenderer::drawVector(f32v2(0.0f), mTileGrid->mAxis[1] * 170.0f, color4(0.0f, 1.0f, 0.0f));
	DebugRenderer::drawVector(f32v2(0.0f), f32v2(0.0f, 1.0f) * 140.0f, color4(0.0f, 0.0f, 1.0f));

	//mEcsRenderer->renderPhysicsDebug(*m_camera2D);
	mEcsRenderer->renderSimpleSprites(*mCamera2D);

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
