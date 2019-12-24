#include "stdafx.h"
#include "MainMenuScreen.h"

#include "App.h"

#include <Vorb/io/IOManager.h>
#include <Vorb/graphics/SpriteBatch.h>
#include <Vorb/graphics/TextureCache.h>
#include <Vorb/ui/InputDispatcher.h>
#include <glm/gtx/rotate_vector.hpp>

#include "Camera2D.h"

#include "TileGrid.h"
#include "Utils.h"

#include "DebugRenderer.h"

MainMenuScreen::MainMenuScreen(const App* app) 
	: IAppScreen<App>(app) {
	m_sb = std::make_unique<vg::SpriteBatch>();
	m_textureCache = std::make_unique<vg::TextureCache>();

	m_ioManager = std::make_unique<vio::IOManager>();

	m_camera2D = std::make_unique<Camera2D>();

	m_tileGrid = std::make_unique<TileGrid>(i32v2(50, 50), *m_textureCache, "data/textures/tiles.png", i32v2(10, 16), 60.0f);
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
	m_sb->init();
	m_textureCache->init(m_ioManager.get());

	m_circleTexture = m_textureCache->addTexture("data/textures/circle.png");

	const f32v2 screenSize(m_app->getWindow().getWidth(), m_app->getWindow().getHeight());
	m_camera2D->init((int)screenSize.x, (int)screenSize.y);

	vui::InputDispatcher::key.onKeyDown.addFunctor([this](Sender sender, const vui::KeyEvent& event) {
		
	});

	vui::InputDispatcher::mouse.onWheel.addFunctor([this](Sender sender, const vui::MouseWheelEvent& event) {
		m_scale = glm::clamp(m_scale + event.dy * 0.05f, 0.5f, 1.5f);
		m_camera2D->setScale(m_scale);
	});

	vui::InputDispatcher::mouse.onButtonDown.addFunctor([this](Sender sender, const vui::MouseButtonEvent& event) {
		m_testClick = m_camera2D->convertScreenToWorld(f32v2(event.x, event.y));

		int tileIndex = m_tileGrid->getTileIndexFromScreenPos(m_testClick, *m_camera2D);
		m_tileGrid->setTile(tileIndex, TileGrid::STONE_1);

	});
}

void MainMenuScreen::destroy(const vui::GameTime& gameTime) {
	m_sb->dispose();
	m_textureCache->dispose();
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
		m_camera2D->offsetPosition(offset);
	}

	m_camera2D->update();
}

void MainMenuScreen::draw(const vui::GameTime& gameTime)
{
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	m_tileGrid->draw(*m_camera2D);

	m_sb->begin();
	m_sb->draw(m_circleTexture.id, m_testClick - f32v2(8.0f), f32v2(16.0f), color4(1.0f, 0.0f, 1.0f));
	m_sb->end();
	m_sb->render(f32m4(1.0f), m_camera2D->getCameraMatrix());

	DebugRenderer::drawVector(f32v2(0.0f), m_tileGrid->m_axis[0] * 170.0f, color4(1.0f, 0.0f, 0.0f));
	DebugRenderer::drawVector(f32v2(0.0f), m_tileGrid->m_axis[1] * 170.0f, color4(0.0f, 1.0f, 0.0f));
	DebugRenderer::drawVector(f32v2(0.0f), f32v4(m_tileGrid->m_axis[0] * 170.0f, 1.0f, 1.0f) * m_tileGrid->m_invIsoTransform, color4(1.0f, 1.0f, 0.0f));
	DebugRenderer::drawVector(f32v2(0.0f), glm::rotate(m_tileGrid->m_axis[1] * 170.0f, glm::radians(90.0f)), color4(1.0f, 1.0f, 1.0f));
	DebugRenderer::drawVector(f32v2(0.0f), f32v2(0.0f, 1.0f) * 140.0f, color4(0.0f, 0.0f, 1.0f));

	DebugRenderer::renderLines(m_camera2D->getCameraMatrix());
}
