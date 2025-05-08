#pragma once

#include "boost/circular_buffer.hpp"

enum class WorldAnalyticEventType : ui8{
    ImmigrationBySea,
    COUNT
};

struct WorldAnalyticEvent {
    WorldAnalyticEvent() = default;
    WorldAnalyticEvent(TimestampMs timestamp, WorldAnalyticEventType type, ui32 value) : timestamp(timestamp), type(type), value(value) {}

    TimestampMs timestamp;
    WorldAnalyticEventType type;
    ui32 value;
};

struct SimWorldAnalyticsData {
    i32 totalPopulation = 0;
    i32 desiredPopulation = 0;
    i32 totalStructures = 0;
    boost::circular_buffer<WorldAnalyticEvent> analyticsHistory;
};

class SimWorldAnalytics
{
public:
    SimWorldAnalytics();

    void adjustTotalPopulation(i32 delta);
    void adjustDesiredPopulation(i32 delta);
    void setDesiredPopulation(i32 desiredPopulation);
    void adjustTotalStructures(i32 delta);
    void addAnalyticsEvent(WorldAnalyticEvent event);

    const SimWorldAnalyticsData& getAnalyticsDataSimThread() const { ASSERT_SIM_THREAD(); return mData; }
    SimWorldAnalyticsData getAnalyticsCopy() const;

protected:
    mutable std::mutex mAnalyticsMutex;
    SimWorldAnalyticsData mData;
};

