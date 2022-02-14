#pragma once

struct OwnershipComponent {
    std::vector<CityPlot*> mOwnedPlots;
    std::vector<ItemStockpile*> mOwnedStockpiles;
};