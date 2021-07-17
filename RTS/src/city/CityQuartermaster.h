#pragma once

class ItemStockpile;
class City;

// Tracks public stockpiles in the city and managers
// resource allocation and trade between cities
// TODO: Should trade be separated?
class CityQuartermaster {
public:
    CityQuartermaster(City& city);

    // creates an unowned stockpile, returns false if conflicts with existing stockpile
    bool tryCreateCityStockpileAt(const ui32AABB& aabb);


private:
    bool checkStockpileOverlap(const ui32AABB& aabb) const;
    // TODO: Sorted with Fast AABB search algorithm?
    std::vector<std::unique_ptr<ItemStockpile>> mAllStockpiles;
    City& mCity;
};
