#include "stdafx.h"
#include "WorldEditorScreen.h"

#include "App.h"

#include "rendering/RenderContext.h"
#include "World.h"
#include "ResourceManager.h"
#include "editor/WorldEditor.h"

#include <Vorb/math/VorbMath.hpp>
#include <Vorb/ui/InputDispatcher.h>
#include <Vorb/graphics/SpriteFont.h>
#include <Vorb/graphics/TextureCache.h>
#include <glm/gtx/rotate_vector.hpp>

#include "Camera2D.h"

// size of global cached random table
const unsigned CACHED_RANDOM_SIZE = 65536;

WorldEditorScreen::WorldEditorScreen(const App* app)
    : IAppScreen<App>(app),
    mResourceManager(std::make_unique<ResourceManager>()),
    mRenderContext(RenderContext::initInstance(*mResourceManager, *mWorld, f32v2(m_app->getWindow().getWidth(), m_app->getWindow().getHeight()))),
    mWorld(std::make_unique<World>(*mResourceManager)) {
	mCamera2D = std::make_unique<Camera2D>();

    // World editor will control the camera for us
    mWorldEditor = std::make_unique<WorldEditor>(*mWorld, static_cast<SDL_Window*>(m_app->getWindow().getHandle()), *mCamera2D);

    // Starting time of day to noon
    mWorld->setTimeOfDay(12.0f);
}


WorldEditorScreen::~WorldEditorScreen() {
}

i32 WorldEditorScreen::getNextScreen() const {
	return 0;
}

i32 WorldEditorScreen::getPreviousScreen() const {
	return 0;
}

void WorldEditorScreen::build() {

	const f32v2 screenSize(m_app->getWindow().getWidth(), m_app->getWindow().getHeight());
    mCamera2D->init((int)screenSize.x, (int)screenSize.y);

    mResourceManager->gatherFiles("data");
    mResourceManager->loadFiles();

    mRenderContext.initPostLoad();
    mResourceManager->writeDebugAtlas();

    mWorld->initPostLoad();

    mWorldEditor->init();

    vui::InputDispatcher::key.onKeyDown.addFunctor([this](Sender sender, const vui::KeyEvent& event) {
        // View toggle
        if (event.keyCode == VKEY_B) {
            sDebugOptions.mWireframe = !sDebugOptions.mWireframe;
        }
        else if (event.keyCode == VKEY_C) {
            sDebugOptions.mChunkBoundaries = !sDebugOptions.mChunkBoundaries;
        }
    });

}

void WorldEditorScreen::destroy(const vui::GameTime& gameTime) {

}

void WorldEditorScreen::onEntry(const vui::GameTime& gameTime) {
}

void WorldEditorScreen::onExit(const vui::GameTime& gameTime) {
}

void WorldEditorScreen::update(const vui::GameTime& gameTime) {

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


    mCamera2D->update();

    mWorld->update(deltaTime, mCamera2D->getPosition(), *mCamera2D);

	// Update
	sFps = vmath::lerp(sFps, m_app->getFps(), 0.85f);
	mFps = sFps;

}

void WorldEditorScreen::draw(const vui::GameTime& gameTime)
{

    mRenderContext.renderFrame(*mCamera2D);

    mWorldEditor->updateAndRender();
}
