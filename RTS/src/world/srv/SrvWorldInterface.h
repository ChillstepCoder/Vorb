#pragma once

class NavWorld;
class IWorld;
class ItemStockpileRegistry;
class FishEcosystem;

class SrvWorldInterface
{
public:
    SrvWorldInterface();
    virtual ~SrvWorldInterface();

    void tickSrv(f32 elapsedSec);

    ItemStockpileRegistry& getItemStockpileRegistry() const { return *mItemStockpileRegistry; }
    NavWorld& getNavWorld() const { return *mNavWorld; }
    FishEcosystem& getFishEcosystem() const { return *mFishEcosystem; }

protected:
    void initSrv(IWorld& world);

    // Stockpiles
    std::unique_ptr<ItemStockpileRegistry> mItemStockpileRegistry;

    // Ecosystems
    std::unique_ptr<FishEcosystem> mFishEcosystem;

    // Nav graph
    std::unique_ptr<NavWorld> mNavWorld;
};

