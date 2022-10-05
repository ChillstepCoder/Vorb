#include "stdafx.h"
#include "ScreenState.h"

bool MainMenuScreenState::isHost = true;
bool MainMenuScreenState::isLan = false;
bool MainMenuScreenState::isSinglePlayer = true;
nString MainMenuScreenState::hostIP;

void MainMenuScreenState::initDefaults() {
    isHost = true;
    isLan = false;
    isSinglePlayer = true;
}

void MainMenuScreenState::setJoin(const nString& hostIp) {
    isHost = false;
    isSinglePlayer = false;
    MainMenuScreenState::hostIP = hostIP;
}

void MainMenuScreenState::setHostLan() {
    isLan = true;
    isSinglePlayer = false;
}

void MainMenuScreenState::setHostOnline() {
    isLan = false;
    isSinglePlayer = false;
}

