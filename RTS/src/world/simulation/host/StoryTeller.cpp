#include "stdafx.h"
#include "StoryTeller.h"

constexpr ui32 MAX_EVENT_HISTORY = 16;

StoryTeller::StoryTeller() : mEventHistory(MAX_EVENT_HISTORY) {

}

StoryTeller::~StoryTeller() {

}

void StoryTeller::setMode(StoryTellerMode mode) {
    mMode = mode;
}

void StoryTeller::tick(f32 elapsedSec) {
    switch (mMode) {
        case StoryTellerMode::HistoryGeneration:
            tickHistoryGeneration(elapsedSec);
            break;
        case StoryTellerMode::Game:
            tickGame(elapsedSec);
            break;
        default:
            assert(false);
            break;
    }
}

void StoryTeller::tickHistoryGeneration(f32 elapsedSec) {

}

void StoryTeller::tickGame(f32 elapsedSec) {

}
