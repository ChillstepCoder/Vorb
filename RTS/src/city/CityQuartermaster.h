#pragma once

class ItemStockpile;

// Tracks public stockpiles in the city and managers
// resource allocation and trade between cities
// TODO: Should trade be separated?
class CityQuartermaster {
public:
    CityQuartermaster();

    // TODO: Fast AABB search algorithm?
    // Sorted?
    std::vector<ItemStockpile*> mAllStockpiles;
};

