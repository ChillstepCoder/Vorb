#include "stdafx.h"
#include "ScreenState.h"

bool MainMenuScreenState::isHost = true;
bool MainMenuScreenState::isLan = false;
bool MainMenuScreenState::isSinglePlayer = true;

void MainMenuScreenState::initDefaults() {
    isHost = true;
    isLan = false;
    isSinglePlayer = true;
}

void MainMenuScreenState::setJoin() {
    isHost = false;
    isSinglePlayer = false;
}

void MainMenuScreenState::setHostLan() {
    isLan = true;
    isSinglePlayer = false;
}

void MainMenuScreenState::setHostOnline() {
    isLan = false;
    isSinglePlayer = false;
}

