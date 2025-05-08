#include "stdafx.h"
#include "SimWorldAnalytics.h"

constexpr size_t MAX_ANALYTICS_EVENTS = 512;

SimWorldAnalytics::SimWorldAnalytics() {
    mData.analyticsHistory = boost::circular_buffer<WorldAnalyticEvent>(MAX_ANALYTICS_EVENTS);
}

void SimWorldAnalytics::adjustTotalPopulation(i32 delta) {
    std::lock_guard lock(mAnalyticsMutex);
    mData.totalPopulation += delta;
}

void SimWorldAnalytics::adjustDesiredPopulation(i32 delta) {
    std::lock_guard lock(mAnalyticsMutex);
    mData.desiredPopulation += delta;
}

void SimWorldAnalytics::setDesiredPopulation(i32 desiredPopulation) {
    std::lock_guard lock(mAnalyticsMutex);
    mData.desiredPopulation = desiredPopulation;
}

void SimWorldAnalytics::adjustTotalStructures(i32 delta) {
    std::lock_guard lock(mAnalyticsMutex);
    mData.totalStructures += delta;
}

void SimWorldAnalytics::addAnalyticsEvent(WorldAnalyticEvent event) {
    std::lock_guard lock(mAnalyticsMutex);

    // Handle effects
    switch (event.type) {
        case WorldAnalyticEventType::ImmigrationBySea:
            mData.totalPopulation += event.value;
            break;
        default:
            assert(false);
            break;
    }
    static_assert(e_count(WorldAnalyticEventType) == 1, "Missing analytic event type");

    // Track history
    mData.analyticsHistory.push_back(event);
}

SimWorldAnalyticsData SimWorldAnalytics::getAnalyticsCopy() const {
    std::lock_guard lock(mAnalyticsMutex);
    return mData;
}
