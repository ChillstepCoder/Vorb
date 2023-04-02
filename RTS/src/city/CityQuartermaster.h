#pragma once

class City;
class BuildingBlueprint;

#include "item/ItemStockpile.h"

// Tracks public stockpiles in the city and managers
// resource allocation and trade between cities
// TODO: Should trade be separated?
class CityQuartermaster {
    friend class CityDebugRenderer;
public:
    CityQuartermaster(City& city);
    ~CityQuartermaster();

    void createStockpilesForBlueprint(BuildingBlueprint& bp);

    // creates an unowned stockpile, returns false if conflicts with existing stockpile
    bool tryCreateCityStockpileAt(const i32AABB2& aabb, entt::entity ownerEntity);
    bool tryCreateCityStockpileAt(const i32AABB2& aabb, bool* ownershipMask, entt::entity ownerEntity);

    ItemStockpile* tryGetClosestStockpileToPoint(const i32v2 position);

    const std::set<ItemStockpile*>& getStockpiles() const { return mAllStockpiles; }

private:
    void initEventHandlers();

    // TODO: Sorted with Fast AABB search algorithm?
    ItemStockpileListeners mItemStockpileListeners;
    std::set<ItemStockpile*> mAllStockpiles;
    City& mCity;
};
