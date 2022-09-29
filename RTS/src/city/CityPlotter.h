#pragma once

#include "city/Building.h"
#include "city/CityPlot.h"
#include "city/CityDistrict.h"

class City;

constexpr int DISTRICT_GRID_WIDTH = 9;
constexpr int DISTRICT_GRID_SIZE = DISTRICT_GRID_WIDTH * DISTRICT_GRID_WIDTH;

class CityPlotter
{
    friend class CityDebugRenderer;
public:
    CityPlotter(City& city);
    ~CityPlotter();

    void initAsTier(int tier);

    void upgradeTier();

    // Gets all currently valid plots for a particular building
    // If no valid plot exists, will automatically grow.
    //std::vector<CityPlot*> getValidPlots(ui32v2 dims);

    CityPlot* tryReservePlotForBuilding(const ui32v2& minBuildingDims, const ui32v2& maxBuildingDims);

private:
    CityDistrict* addDistrict(DistrictType type, CityDistrict* parent, ui32 size);

    void generateAlleysForUnroadedPlots(CityDistrict& newDistrict);

    bool markDistrictTilesAsOwned(CityDistrict& district); // Returns false if there was a conflict
    CityPlot* addPlot(ui32v2 dims);

    // axis 0 = horizontal, 1 = vertical
    void addRoad(CityDistrict& district, ui32v2 startPos, ui32v2 endPos, ui32 width, int axis);

    // Takes a plot and an input AABB, then splits the plot into sub plots by subtracting the AABB volume.
    // Returns false if we deleted the plot and need to try to split this index again
    bool splitPlotByAABBIntersect(CityPlotIndex plotIndex, const i32AABB2& aabb, OPT CityRoad* road);

    // Split a plot into two plots. Returns index of new plot.
    // axis 0 = split horizontally, 1 = split vertically
    CityPlotIndex splitPlotAlongAxis(ui32v2 splitPoint, CityPlotIndex plot, int axis, RoadID roadID);
    void tryConnectRoad(CityPlotIndex plotIndex, const i32AABB2& roadAabb, RoadID roadID);

    // First plot is root plot
    std::vector<std::unique_ptr<CityPlot>> mPlots;
    //std::vector<CityPlotIndex> mFreePlots;
    std::vector<std::unique_ptr<CityDistrict>> mDistricts;

    CityDistrict* mDistrictGrid[DISTRICT_GRID_SIZE] = {};

    ui32 mCurrentTier = UINT_MAX;

    City& mCity;

};

