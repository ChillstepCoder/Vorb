#pragma once

struct Building;

class CityItemManager
{
public:
    Building* getBestStockpileForItem();

    std::vector<Building*> mItemStockpiles;
};

