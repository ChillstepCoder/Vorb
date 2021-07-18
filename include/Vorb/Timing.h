//
// Timing.h
// Vorb Engine
//
// Created by Cristian Zaloj on 8 Dec 2014
// Copyright 2014 Regrowth Studios
// MIT License
//

/*! \file Timing.h
 * @brief Different timers.
 */

#pragma once

#ifndef Vorb_Timing_h__
//! @cond DOXY_SHOW_HEADER_GUARDS
#define Vorb_Timing_h__
//! @endcond

#ifndef VORB_USING_PCH
#include <map>
#include <vector>

#include "Vorb/types.h"
#endif // !VORB_USING_PCH

#include <chrono>

// TODO: Instead use steady_clock?
typedef std::chrono::time_point<std::chrono::high_resolution_clock> TimePoint;

// Inteded for use inside the game loop to deterministically count ticks for updates
class TickCounter {
public:
    // For tickPeriod 1 == 0
    TickCounter() = default;
    TickCounter(ui32 tickPeriod, bool tickAtStart);

    // Returns true on the tick period
    // Only call this once per gameplay tick per counter
    bool tryTick();
    void reset();

    f32 getTickDelta() const { return (f32)mCurTick / mTickPeriod; }

protected:
    ui32 mTickPeriod = 0;
    ui32 mCurTick = 0;
};

// Used to synchronize update rates for objects with time
class TickingTimer {
public:
    TickingTimer(f64 msPerTick, f64 maxMSPerFrame);

    void setMsPerTick(f64 msPerTick) { mMsPerTick = msPerTick; }
    void startFrame();
    // Returns true while we should be ticking
    bool tryTick();

    f32 getFrameAlpha() { return (f32)(mAccumulator / mMsPerTick); }

protected:
    TimePoint mCurrTime;
    f64 mAccumulator = 0.0;
    f64 mMsPerTick;
    f64 mMaxMsPerFrame;
};

// Used for delaying a specific event for a period of time. Intended for single use timers
class ResumeTimer {
public:
    ResumeTimer(f64 timeInSeconds);

    bool tryResume();

protected:
    f64 mMsUntilResume;
    TimePoint mStart;
};

class PreciseTimer {
public:
    PreciseTimer() {
        start();
    }
    void start();
    /// Returns time in MS
    f64 stop();

    const bool& isRunning() const {
        return m_timerRunning;
    }
private:
    bool m_timerRunning = false;
    TimePoint m_start;
};

class AccumulationTimer {
public:
    void start(const nString& tag);
    f64 stop();

    void clear();
    void printAll(bool averages);
private:
    class AccumNode {
    public:
        void addSample(f64 sample) {
            numSamples++;
            time += sample;
        }
        i32 numSamples = 0;
        f64 time = 0.0f;
    };

    bool m_timerRunning = false;
    TimePoint m_start;
    std::map<nString, AccumNode> m_accum;
    std::map<nString, AccumNode>::iterator m_it;
};

class MultiplePreciseTimer {
public:
    void start(const nString& tag);
    void stop();

    /// Prints all timings
    void end(const bool& print);
    void setDesiredSamples(const i32& desiredSamples) {
        m_desiredSamples = desiredSamples;
    }
private:
    struct Interval {
        Interval(const nString& Tag) :
            tag(Tag) {
            // Empty
        };
        
        nString tag;
        f64 time = 0.0f;
    };

    i32 m_samples = 0;
    i32 m_desiredSamples = 1;
    ui32 m_index = 0;
    PreciseTimer m_timer;
    std::vector<Interval> m_intervals;
};

/// Calculates FPS 
class FpsCounter {
public:
    /// Begins a frame timing for fps calculation
    void beginFrame();

    /// Ends the frame
    /// @return The current FPS
    f32 endFrame();

    /// Returns the current fps as last recorded by an endFrame
    const f32& getCurrentFps() const {
        return m_fps;
    }
protected:
    // Calculates the current FPS
    void calculateFPS();

    f32 m_fps = 0.0f;
    f32 m_frameTime = 0.0f;
    ui32 m_startTicks = 0;
};

#define DEFAULT_MAX_FPS 60.0f

/// Calculates FPS and also limits FPS
class FpsLimiter : public FpsCounter {
public:
    // Initializes the FPS limiter. For now, this is analogous to setMaxFPS
    void init(const f32& maxFPS);

    // Sets the desired max FPS
    void setMaxFPS(const f32& maxFPS);

    // Limits (blocks) the current frame to achieve the desired FPS
    /// @return Current FPS
    f32 endFrame();
private:
    f32 m_maxFPS = DEFAULT_MAX_FPS;
};

#endif // !Vorb_Timing_h__
