#pragma once

#include "Building.h"
#include "BuildingBlueprintGenerator.h"

#include "tile/TileConst.h"

class City;
class CityBuilder;
struct CityPlot;
// ***********************************************************************************************************
// The city planner handles determining where current and future buildings should be placed.
// and what types of buildings should be built.
// The CityBuilder will query plans from the CityPlanner as needed.
// The CityPlanner can also send urgent plans to the CityBuilder to override existing plans

// It will need to be dynamic, for example a town in a peaceful area will not allocate much into military structures,
// while if the threat in an area is large, it may decide to add walls sooner

struct PlotRequestProps {
    std::vector<TileResource> proximityResources; // Nearby resources that we want
    DistrictType desiredDistrict = DistrictType::NONE;
    bool isDistrictMandatory = false;
    ui32v2 minBuildingDims = ui32v2(5);
    ui32v2 maxBuildingDim = ui32v2(40);
    // f32 budget; // How much we are willing to pay (Can go into debt)
};

class CityPlanner
{
    friend class CityDebugRenderer;
public:
    CityPlanner(City& city);

    void update();

    CityPlot* tryPurchasePlot(const PlotRequestProps& props, entt::entity newOwner);
    void generatePlanForPlotAsyncThenSendToBuilder(CityPlot& plot, const nString& buildingDescriptionName, BuildingBlueprintFlags flags);

    void debugPrintBlueprint(std::unique_ptr<BuildingBlueprint>& bp) const;
private:


    std::unique_ptr<BuildingBlueprintGenerator> mBlueprintGenerator;

    City& mCity;
};

