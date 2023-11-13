#include "stdafx.h"
#include "ScreenState.h"

ServerType MainMenuScreenGlobalState::serverType = ServerType::NONE;
nString MainMenuScreenGlobalState::hostIP;

bool GameplayScreenGlobalState::isQuittingToMenu = false;
bool GameplayScreenGlobalState::isQuittingToDesktop = false;

void MainMenuScreenGlobalState::initDefaults() {
    serverType = ServerType::NONE;
    hostIP = "";
}

void MainMenuScreenGlobalState::setJoin(const nString& targetHostIp) {
    serverType = ServerType::NONE;
    MainMenuScreenGlobalState::hostIP = targetHostIp;
}

void MainMenuScreenGlobalState::setHostLan() {
    serverType = ServerType::LAN;
    hostIP = "";
}

void MainMenuScreenGlobalState::setHostOnline() {
    serverType = ServerType::ONLINE;
    hostIP = "";
}

void MainMenuScreenGlobalState::setHostDev()
{
    serverType = ServerType::DEV;
    hostIP = "";
}

void GameplayScreenGlobalState::initDefaults() {
    isQuittingToMenu = false;
    isQuittingToDesktop = false;
}