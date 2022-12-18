#include "stdafx.h"
#include "MainMenuScreen.h"

#include "App.h"


#include "ui/ImguiUtil.hpp"

#include <Vorb/ui/imgui/imgui.h>
#include <Vorb/ui/imgui/backends/imgui_impl_sdl.h>
#include <Vorb/ui/imgui/backends/imgui_impl_opengl3.h>

#include <Vorb/graphics/DepthState.h>
#include <Vorb/graphics/BlendState.h>

#include "screens/ScreenState.h"
#include "network/cli/GameClient.h"

MainMenuScreen::MainMenuScreen(App* const app) : IAppScreen<App>(app) {
    MainMenuScreenGlobalState::initDefaults();
}

MainMenuScreen::~MainMenuScreen()
{

}

i32 MainMenuScreen::getNextScreen() const
{
    return 1;
}

i32 MainMenuScreen::getPreviousScreen() const
{
    return 0;
}

void MainMenuScreen::build()
{

}

void MainMenuScreen::destroy(const vui::GameTime& gameTime)
{

}

void MainMenuScreen::onEntry(const vui::GameTime& gameTime) {
    LOG_CRITICAL("===Diplaying TODO Messages===");
    LOG_CRITICAL("  TODO: Make terrain color match grass color");
    LOG_CRITICAL("  TODO: Optimize character renderer with UBO");
    LOG_CRITICAL("  TODO: Conan package manager");
    LOG_CRITICAL("  TODO: Update cmake");
    LOG_CRITICAL("  TODO: Custom allocator for std::string/nString");
    // TODO: Vertex pooling https://nickmcd.me/2021/04/04/high-performance-voxel-engine/
    // TODO: Spatial hash grid? https://github.com/simondevyoutube/Quick_3D_MMORPG/blob/main/client/shared/spatial-hash-grid.mjs

    // TODO: Visibility buffer? http://filmicworlds.com/blog/visibility-buffer-rendering-with-material-graphs/
}

void MainMenuScreen::onExit(const vui::GameTime& gameTime)
{

}

void MainMenuScreen::update(const vui::GameTime& gameTime)
{

}

const ImVec2 buttonSize(200, 50);

void MainMenuScreen::draw(const vui::GameTime& gameTime)
{
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    vg::DepthState::NONE.set();
    vg::BlendState::set(vg::BlendStateType::ALPHA);

    glClearColor(0.5f, 0.5f, 0.5f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    vui::GameWindow& window = m_app->getWindow();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame((SDL_Window*)window.getHandle());
    ImGui::NewFrame();

    constexpr float WINDOW_WIDTH = 400.0f;
    constexpr float WINDOW_HEIGHT = 300.0f;
    const ui32v2 screenDims = window.getViewportDims();
    ImGui::SetNextWindowPos(ImVec2(screenDims.x * 0.5f - WINDOW_WIDTH * 0.5f, screenDims.y * 0.5 - WINDOW_HEIGHT * 0.5f));
    ImGui::SetNextWindowSize(ImVec2(WINDOW_WIDTH, WINDOW_HEIGHT));

    ImGui::Begin("MAIN MENU", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar);
    ui32 ID = 10;

    switch (mState) {
        case MainMenuState::MAIN:
            drawMainState();
            break;
        case MainMenuState::MULTIPLAYER:
            drawMultiplayerState();
            break;
        case MainMenuState::JOIN:
            drawJoinState();
            break;
        case MainMenuState::LAN_JOIN:
            drawLanJoinState();
            break;
        case MainMenuState::ONLINE_JOIN:
            drawOnlineJoinState();
            break;
        case MainMenuState::WAITING_JOIN:
            drawWaitingJoinState();
            break;
        case MainMenuState::FAILED_TO_CONNECT:
            drawFailedToConnectState();
            break;
        case MainMenuState::HOST:
            drawHostState();
            break;
        case MainMenuState::OPTIONS:
            break;
        default:
            break;

    }
    static_assert(e_cast(MainMenuState::COUNT) == 9);

    ImGui::End();


    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    ImGui::EndFrame();
}


void MainMenuScreen::attemptConnect(ServerType serverType) {
    mTargetHostAddress = yojimbo::Address(mTargetHostIP.c_str(), DEFAULT_SERVER_PORT);

    if (!mTargetHostAddress.IsValid()) {
        mErrorString = "Malformed address, please use correct host IP\n Example ipv4: 14.12.163.924\n Example ipv6: 2101:602:a07e:c230:5bc:d26b:8407:502b";
        mState = MainMenuState::FAILED_TO_CONNECT;
        return;
    }
    // TODO: Assert well formatted IP
    std::cout << "Attempting to connect to " << mTargetHostIP << std::endl;
    mState = MainMenuState::WAITING_JOIN;
    GameClient& gameClient = GameClient::initInstance(serverType, mTargetHostAddress);
    gameClient.connect(DEFAULT_PRIVATE_KEY);
    mConnectingStart = yojimbo_time();
    mConnTimer = mConnectingStart;
}

void MainMenuScreen::drawMainState() {
    ImGui::Spacing();
    if (ImguiUtil::ButtonCenteredOnLine("Singleplayer", buttonSize)) {
        clearTextInputBuffer();
        m_state = vui::ScreenState::CHANGE_NEXT;
    }
    ImGui::Spacing();
    if (ImguiUtil::ButtonCenteredOnLine("Multiplayer", buttonSize)) {
        mState = MainMenuState::MULTIPLAYER;
    }
    ImGui::Spacing();
    if (ImguiUtil::ButtonCenteredOnLine("Options", buttonSize)) {

    }
    ImGui::Spacing();
    if (ImguiUtil::ButtonCenteredOnLine("Exit", buttonSize)) {
        m_state = vui::ScreenState::EXIT_APPLICATION;
    }
}

void MainMenuScreen::drawMultiplayerState() {
    // Just always clear it here
    clearTextInputBuffer();

    ImGui::Text("Multiplayer");
    ImGui::Spacing();
    if (ImguiUtil::ButtonCenteredOnLine("Host game", buttonSize)) {
        clearTextInputBuffer();
        mState = MainMenuState::HOST;
    }
    ImGui::Spacing();
    if (ImguiUtil::ButtonCenteredOnLine("Join game", buttonSize)) {
        clearTextInputBuffer();
        mState = MainMenuState::JOIN;
    }
    ImGui::Spacing();
    if (ImguiUtil::ButtonCenteredOnLine("Back", buttonSize)) {
        mState = MainMenuState::MAIN;
    }
}

void MainMenuScreen::drawJoinState() {
    ImGui::Text("Join game");
    ImGui::Spacing();
    if (ImguiUtil::ButtonCenteredOnLine("LAN", buttonSize)) {
        mState = MainMenuState::LAN_JOIN;
    }
    ImGui::Spacing();
    if (ImguiUtil::ButtonCenteredOnLine("Online", buttonSize)) {
        mState = MainMenuState::ONLINE_JOIN;
    }
    ImGui::Spacing();
    if (ImguiUtil::ButtonCenteredOnLine("DEV", buttonSize)) {
        mTargetHostIP = "127.0.0.1";
        attemptConnect(ServerType::DEV);
    }
    ImGui::Spacing();
    if (ImguiUtil::ButtonCenteredOnLine("Back", buttonSize)) {
        mState = MainMenuState::MULTIPLAYER;
    }
}

void MainMenuScreen::drawLanJoinState() {
    ImGui::Text("Enter Local IP of Host");
    ImGui::Spacing();
    ImGui::InputText("IP", mTextInputBuffer, 256);
    if (ImguiUtil::ButtonCenteredOnLine("Join", buttonSize)) {
        mTargetHostIP = mTextInputBuffer;
        attemptConnect(ServerType::LAN);
    }
    if (ImguiUtil::ButtonCenteredOnLine("Back", buttonSize)) {
        mState = MainMenuState::JOIN;
    }
}

void MainMenuScreen::drawOnlineJoinState() {
    ImGui::Text("Enter IPv6 of Host");
    ImGui::Spacing();
    ImGui::InputText("IP", mTextInputBuffer, 256);
    if (ImguiUtil::ButtonCenteredOnLine("Join", buttonSize)) {
        mTargetHostIP = mTextInputBuffer;
        attemptConnect(ServerType::ONLINE);
    }
    if (ImguiUtil::ButtonCenteredOnLine("Back", buttonSize)) {
        mState = MainMenuState::JOIN;
    }
}

void MainMenuScreen::drawHostState() {
    ImGui::Text("Host game");
    ImGui::Spacing();
    if (ImguiUtil::ButtonCenteredOnLine("LAN", buttonSize)) {
        MainMenuScreenGlobalState::setHostLan();
        m_state = vorb::ui::ScreenState::CHANGE_NEXT;
    }
    ImGui::Spacing();
    if (ImguiUtil::ButtonCenteredOnLine("Online", buttonSize)) {
        MainMenuScreenGlobalState::setHostOnline();
        m_state = vorb::ui::ScreenState::CHANGE_NEXT;
    }
    ImGui::Spacing();
    if (ImguiUtil::ButtonCenteredOnLine("DEV", buttonSize)) {
        MainMenuScreenGlobalState::setHostDev();
        m_state = vorb::ui::ScreenState::CHANGE_NEXT;
    }
    ImGui::Spacing();
    if (ImguiUtil::ButtonCenteredOnLine("Back", buttonSize)) {
        mState = MainMenuState::MULTIPLAYER;
    }
}

void MainMenuScreen::drawWaitingJoinState() {
    char buf[256];
    sprintf_s(buf, "Joining server: %s", mTargetHostIP.c_str());
    ImGui::Text(buf);
    ImGui::Spacing();
    if (ImguiUtil::ButtonCenteredOnLine("Back", buttonSize)) {
        GameClient::destroyInstance();
        mState = MainMenuState::JOIN;
        return;
    }

    double currentTime = yojimbo_time();
    // Check timeout
    if (currentTime - mConnectingStart >= YOJIMBO_DEFAULT_TIMEOUT) {
        mErrorString = "Connection timed out";
        mState = MainMenuState::FAILED_TO_CONNECT;
        GameClient::destroyInstance();
        return;
    }

    // Update client packets
    // TODO: We need to make sure our local IP is same protocol as host IP, i.e. ipv4 or ipv6
    GameClient& client = GameClient::getInstance();
    double dt = currentTime - mConnTimer;
    client.update(dt);
    mConnTimer = currentTime;
    
    if (client.isConnected()) {
        // Join host game
        MainMenuScreenGlobalState::setJoin(mTargetHostIP);
        m_state = vorb::ui::ScreenState::CHANGE_NEXT;
    }
}

void MainMenuScreen::drawFailedToConnectState() {
    char buf[256];
    sprintf_s(buf, "FAILED TO JOIN SERVER: %s", mTargetHostIP.c_str());
    ImGui::Text(buf);
    ImGui::Spacing();
    ImGui::Text(mErrorString.c_str());
    ImGui::Spacing();
    if (ImguiUtil::ButtonCenteredOnLine("Back", buttonSize)) {
        clearTextInputBuffer();
        mState = MainMenuState::JOIN;
    }
}

void MainMenuScreen::clearTextInputBuffer() {
    mTextInputBuffer[0] = '\0';
}
