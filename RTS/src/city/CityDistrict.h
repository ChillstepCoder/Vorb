#pragma once

#include "city/Building.h"

// Collection of plots and streets that represent a themed area of a city,
// Which represents the overarching structure of a city
// TODO: Polygon shape? Merging?
struct CityDistrict {
    i32AABB2 aabb;
    DistrictType type;
    CityDistrict* children[4]; // S,W,E,N
    CityDistrict* parent = nullptr;
    Cartesian parentDirection;
    int numChildren = 0;
    int districtGridIndex = INT_MAX;

    //std::vector<ActorId> mResidents;
    std::vector<Building> buildings;
    std::vector<RoadID> roads;
};