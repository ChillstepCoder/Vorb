#pragma once

#include "World/GridID.h"
#include "boost/container/flat_set.hpp"

enum class SimChunkState : ui8 {
    Dormant,
    Simulating,
    Full,
    COUNT
};
static_assert(e_count(SimChunkState) <= 4, "Fit in two bits");

class SimChunkData {
    boost::container::flat_set<entt::entity> mPeople;
    FactionID mOwnerFaction;
    entt::entity mOwnerSettlement;
};
static_assert(sizeof(SimChunkData) == 32, "Keep small");
