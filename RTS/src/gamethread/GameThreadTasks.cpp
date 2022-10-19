#include "stdafx.h"
#include "GameThreadTasks.h"

GameThreadTasks* GameThreadTasks::sInstance = nullptr;

GameThreadTasks::GameThreadTasks() {

}

GameThreadTasks::~GameThreadTasks() {

}

GameThreadTasks& GameThreadTasks::initInstance() {
    if (!sInstance) {
        sInstance = new GameThreadTasks();
    }
    return *sInstance;
}

GameThreadTasks& GameThreadTasks::getInstance() {
    assert(sInstance);
    return *sInstance;
}

