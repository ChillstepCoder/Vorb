#pragma once

class NavWorld;
class IWorld;
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
    void initSrv(IWorld& world);

    // Stockpiles
    std::unique_ptr<ItemStockpileRegistry> mItemStockpileRegistry;

    // Nav graph
    std::unique_ptr<NavWorld> mNavWorld;
};

