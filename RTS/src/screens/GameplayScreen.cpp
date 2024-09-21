#include "stdafx.h"
#include "screens/GameplayScreen.h"

#include "App.h"

#include "camera/CameraController.h"
#include "gamethread/GameThread.h"
#include "input/InputDispatcher.h"
#include "item/ItemStockpileRegistry.h"
#include "network/cli/CliMessage.h"
#include "network/cli/GameClient.h"
#include "network/srv/GameServerOLD.h"
#include "options/DebugOptions.h"
#include "physics/PhysicsWorld.h"
#include "rendering/LoadScreenRenderer.h"
#include "rendering/RenderContext.h"
#include "resources/ResourceManager.h"
#include "screens/ScreenState.h"
#include "ui/UIContext.h"
#include "world/controller/EditorWorldInterfaceController.h"

constexpr ui32 MAX_TICKS_PER_UPDATE = 3;
constexpr f64 TICK_RATE_MS = 40.0;

#define WRITE_DEBUG_ATLAS 0

GameplayScreen::GameplayScreen(App* const app)
	: IAppScreen<App>(app), mResourceManager(Services::ResourceManager::ref()) {

    // TODO: Toolchain - https://www.youtube.com/watch?v=550brv-VBgE
    // https://www.youtube.com/c/Progrematic/videos

    mRenderContext = &RenderContext::initInstance(f32v2(m_app->getWindow().getWidth(), m_app->getWindow().getHeight()), static_cast<SDL_Window*>(m_app->getWindow().getHandle()));

    // TODO: Config
    sDebugOptions.mVSYNC = m_app->getWindow().getSwapInterval() == vui::GameSwapInterval::V_SYNC;

    // TODO: This is kinda stupid
    /*if (WeaponRegistry::s_allWeaponItems.empty()) {
        ArmorRegistry::loadArmors();
        WeaponRegistry::loadWeapons();
        ShieldRegistry::loadShields();
    }*/

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
	return e_cast(RegisteredScreens::MainMenu);
}

i32 GameplayScreen::getPreviousScreen() const {
	return e_cast(RegisteredScreens::MainMenu);
}

void GameplayScreen::build() {

    PhysicsWorld::initializeJPH();

    mResourceManager.setResourceRoot("data", "_cache");

	const f32v2 screenSize(m_app->getWindow().getWidth(), m_app->getWindow().getHeight());

    mResourceManager.gatherFiles();

    // Show loading screen
    LoadScreenRenderer& loadScreenRenderer = LoadScreenRenderer::getInstance();
    loadScreenRenderer.appendLoadingTexture(CStrToken("loading"));
    displayLoadScreen("Loading files...", true);
	mResourceManager.loadFiles();

    {
        ScopedTimer timer("Render context init");
        mRenderContext->initPostLoad();
    }
#if WRITE_DEBUG_ATLAS == 1
    {
        ScopedTimer timer("Write debug atlas");
        mResourceManager.writeDebugAtlas();
    }
#endif
    if (!UIContext::hasInstance()) {
        UIContext::initInstance(f32v2(m_app->getWindow().getWidth(), m_app->getWindow().getHeight()), static_cast<SDL_Window*>(m_app->getWindow().getHandle()));
    }
}

void GameplayScreen::destroy(const vui::GameTime& gameTime) {
    
}

void GameplayScreen::onEntry(const vui::GameTime& gameTime) {

    mState = GameplayScreenState::INIT;

    GameplayScreenGlobalState::initDefaults();

    // Initialize services
    if (MainMenuScreenGlobalState::isClient()) {
        //Services::initCli();

        mState = GameplayScreenState::WAITING_JOIN_SERVER;
        CliMessage::sendClientReadyJoinMessage();
        // Send the packet
        GameClient::getInstance().update(0.0f);
        mNetMode = WorldNetMode::Client;
    }
    else {
        //Services::initHost();

        mState = GameplayScreenState::RUNNING;
        mNetMode = WorldNetMode::Host;
    }

    // Initialize hosted server if needed
    if (MainMenuScreenGlobalState::serverType != ServerType::NONE) {
        GameServerOLD::initInstance(*sGameWorld, MainMenuScreenGlobalState::serverType);
    }

    // Always init the world
    displayLoadScreen("Loading...", true);

    initCamera();

    // Start the game :O
    GameThread::initInstance(*sGameWorld, mNetMode);

    initWorldInterfaceController();

    initEvents();
}

void GameplayScreen::onExit(const vui::GameTime& gameTime) {
    IS_SHUTTING_DOWN = true;
    displayLoadScreen("Cleaning up...", true);
    sGameWorld.reset();
    Services::destroy();
    IS_SHUTTING_DOWN = false;
}

void GameplayScreen::update(const vui::GameTime& gameTime) {
    ASSERT_RENDER_THREAD();

    updateTimeScaling(gameTime);
    if (mWorldInterfaceController) {
        mWorldInterfaceController->update();
    }

    // Check quit
    if (GameplayScreenGlobalState::isQuittingToDesktop) {
        vui::InputDispatcher::onQuit();
        return;
    }
    else if (GameplayScreenGlobalState::isQuittingToMenu) {
        m_state = vorb::ui::ScreenState::CHANGE_PREVIOUS;
        return;
    }

    if (mState == GameplayScreenState::RUNNING) {
        // Update functions
        switch (mNetMode) {
            case WorldNetMode::Client:
                updateClient(gameTime);
                break;
            case WorldNetMode::Host:
                updateHost(gameTime);
                break;
            default:
                assert(false);
                break;
        }
    }
    else if (mState == GameplayScreenState::WAITING_JOIN_SERVER) {
        // Update client and wait for server response
        GameClient& client = GameClient::getInstance();
        if (!client.isConnected()) {
            pError("LOST CONNECTION!");
            vui::InputDispatcher::onQuit();
            return;
        }
        client.update(gameTime.deltaTime);
        // Once server joins us, we are good to go
        if (client.isJoined()) {
            mState = GameplayScreenState::RUNNING;
            initCamera();
        }
    }
}

void GameplayScreen::draw(const vui::GameTime& gameTime) {
    ASSERT_RENDER_THREAD();

    if (mState == GameplayScreenState::RUNNING) {

        const f32 frameAlpha = 0.0f /*TODO: Framealpha?*/;

        // Grab fps
        sFps = util::lerp(sFps, m_app->getFps(), 0.85f);
        mFps = sFps;

        mRenderContext->renderFrame(*mCameraController, frameAlpha, gameTime.elapsedSec);

        if (mWorldInterfaceController) {
            mWorldInterfaceController->renderUI();
        }

        mRenderContext->endFrame();
    }
    else if (mState == GameplayScreenState::WAITING_JOIN_SERVER) {
        displayLoadScreen("Waiting server response...", false);
    }

}

void GameplayScreen::initCamera() {
    mCameraController = std::make_unique<CameraController>(m_app->getWindow());
}

void GameplayScreen::initEvents()
{
    UIContext::getInstance().registerUIContextListeners(mUIListeners);
    UIContext::getInstance().addEditorWorldSetListener(mUIListeners, [this](const UIContextEvent& evnt) {
        ASSERT_RENDER_THREAD();
        //  When there is an editor world, we will disable our controller to allow the editor one to run
        if (evnt.mWorld) {
            mWorldInterfaceController.reset();
        }
        else {
            initWorldInterfaceController();
        }
    });
}

void GameplayScreen::initWorldInterfaceController() {
    mWorldInterfaceController = std::make_unique<EditorWorldInterfaceController>(m_app->getWindow(), *sGameWorld, *mCameraController);
    mWorldInterfaceController->init();
}

void GameplayScreen::updateClient(const vui::GameTime& gameTime) {
    // Update client
    if (!GameClient::getInstance().isConnected()) {
        pError("LOST CONNECTION!");
        assert(false);
    }
}

void GameplayScreen::updateHost(const vui::GameTime& gameTime) {

}

void GameplayScreen::updateTimeScaling(const vui::GameTime& gameTime) {
    // DEBUG Time advance
    /*static constexpr float TIME_ADVANCE_MULT = 4.0f;
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
    }*/
}

void GameplayScreen::displayLoadScreen(const nString& text, bool syncWindow) {
    LoadScreenRenderer& loadScreenRenderer = LoadScreenRenderer::getInstance();
    loadScreenRenderer.setText(text);
    loadScreenRenderer.render(syncWindow ? &m_app->getWindow() : nullptr);
}
