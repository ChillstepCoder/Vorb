#pragma once

class ItemStockpile;
class City;
struct BuildingBlueprint;

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
    bool tryCreateCityStockpileAt(const ui32AABB2& aabb, entt::entity ownerEntity);
    bool tryCreateCityStockpileAt(const ui32AABB2& aabb, bool* ownershipMask, entt::entity ownerEntity);

    ItemStockpile* tryGetClosestStockpileToPoint(const ui32v2 position);

    const std::vector<ItemStockpile*>& getStockpiles() const { return mAllStockpiles; }


private:
    void onStockpileDestroy(Sender s, ItemStockpile* stockPile);

    // TODO: Sorted with Fast AABB search algorithm?
    std::vector<ItemStockpile*> mAllStockpiles;
    City& mCity;
};
