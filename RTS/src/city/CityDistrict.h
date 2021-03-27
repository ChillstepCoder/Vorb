#pragma once

#include "city/Building.h"

// TODO: Data driven
// TODO: District Conversion
// Higher numbers are higher priority. For example, Industrial can replace Rural
enum class DistrictTypes {
    Rural,
    Farming,
    Outpost,
    Residential,
    Industrial,
    Commercial,
    Military,
    Government,
    Harbor,
};

// Collection of plots and streets that represent a themed area of a city,
// Which represents the overarching structure of a city
// TODO: Polygon shape? Merging?
struct CityDistrict {
    ui32v4 aabb;
    DistrictTypes type;
    CityDistrict* children[4]; // S,W,E,N
    CityDistrict* parent = nullptr;
    Cartesian parentDirection;
    int numChildren = 0;
    int districtGridIndex = INT_MAX;

    //std::vector<ActorId> mResidents;
    std::vector<Building> buildings;
    std::vector<RoadID> roads;
};