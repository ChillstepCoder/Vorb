#pragma once

#include "world/simulation/host/storyteller/IStoryTellerEvent.h"
#include <boost/circular_buffer.hpp>

// https://www.reddit.com/r/RimWorld/comments/nmx5bi/which_events_are_triggered_by_storytellers/
struct StoryTellerConfig {
    f32v2 timeBetweenEventsRangeSec = f32v2(60.0f, 600.0f);
    f32 eventDifficultyMultiplier = 1.0f;
    i32 averageCriticalEventFrequency = 10; // One every N events
};

enum class StoryTellerMode {
    HistoryGeneration,
    Game
};

// Decides what to throw at the player, what to do with the world.
// Think of this being the "dungeon master" of the game.
class StoryTeller {
public:
    StoryTeller();
    ~StoryTeller();

    void setMode(StoryTellerMode mode);
    void tick(f32 elapsedSec);

private:
    void tickHistoryGeneration(f32 elapsedSec);
    void tickGame(f32 elapsedSec);

    StoryTellerConfig mConfig;
    StoryTellerMode mMode = StoryTellerMode::HistoryGeneration;

    std::vector<IStoryTellerEventPtr> mActiveEvents;
    boost::circular_buffer<IStoryTellerEventPtr> mEventHistory; // Allows us to keep track of what we've done when deciding what to do next
};

