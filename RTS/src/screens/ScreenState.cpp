#include "stdafx.h"
#include "ScreenState.h"

ServerType MainMenuScreenState::serverType = ServerType::NONE;
nString MainMenuScreenState::hostIP;

bool GameplayScreenState::isQuittingToMenu = false;
bool GameplayScreenState::isQuittingToDesktop = false;

void MainMenuScreenState::initDefaults() {
    serverType = ServerType::NONE;
}

void MainMenuScreenState::setJoin(const nString& hostIp) {
    serverType = ServerType::NONE;
    MainMenuScreenState::hostIP = hostIP;
}

void MainMenuScreenState::setHostLan() {
    serverType = ServerType::LAN;
}

void MainMenuScreenState::setHostOnline() {
    serverType = ServerType::ONLINE;
}

void MainMenuScreenState::setHostDev()
{
    serverType = ServerType::DEV;
}

void GameplayScreenState::initDefaults() {
    isQuittingToMenu = false;
    isQuittingToDesktop = false;
}
