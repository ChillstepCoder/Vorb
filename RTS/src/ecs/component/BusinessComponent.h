#pragma once

#include "city/CityConst.h"
#include "city/Building.h"

class City;
class World;

struct BusinessComponent {
    // TODO: Trade empires? Multi city?
    City* mCity = nullptr;
    std::vector<BuildingID> mBuildings;
    std::vector<entt::entity> mEmployees; // TODO: Death notify
};

struct GatheringBusinessComponent {

};

struct RetailBusinessComponent {

};

struct ProcessingBusinessComponent {

};

class BusinessSystem {
public:
    BusinessSystem(World& world);

    void update(entt::registry& registry, float deltaTime);

    World& mWorld;
    int mFramesUntilUpdate = 0;
};