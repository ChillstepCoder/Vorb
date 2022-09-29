#pragma once

class CityGraph;
class NavWorld;
class ItemStockpileRegistry;

class SrvWorldInterface
{
public:
    SrvWorldInterface();
    virtual ~SrvWorldInterface();

    void tickSrv();

    ItemStockpileRegistry& getItemStockpileRegistry() { return *mItemStockpileRegistry; }
    const ItemStockpileRegistry& getItemStockpileRegistry() const { return *mItemStockpileRegistry; }
    NavWorld& getNavWorld() { return *mNavWorld; }
    const NavWorld& getNavWorld() const { return *mNavWorld; }

protected:
    void updateCities();


    // Stockpiles
    std::unique_ptr<ItemStockpileRegistry> mItemStockpileRegistry;

    // Nav graph
    std::unique_ptr<NavWorld> mNavWorld;
};

