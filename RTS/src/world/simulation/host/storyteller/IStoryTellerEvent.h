#pragma once

class IStoryTellerEvent {
public:
    IStoryTellerEvent() = default;
    virtual ~IStoryTellerEvent() = default;

protected:
    TimestampMs timeFired;
    f32 difficulty = 0.0f; // Negative means it is a "good" event, positive means it is a "bad" event
    f32 historyGenerationProbability = 0.0f; // Probability of this event happening during history generation
    f32 gameProbability = 0.0f; // Probability of this event happening in the game
};
typedef std::shared_ptr<IStoryTellerEvent> IStoryTellerEventPtr;