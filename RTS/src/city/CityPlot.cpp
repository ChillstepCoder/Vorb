#include "stdafx.h"
#include "CityPlot.h"

#include "BuildingBlueprintGenerationContext.h"

CityPlot::CityPlot()
{

}

CityPlot::CityPlot(const i32AABB2& aabb, CityPlotIndex plotIndex, CityDistrict* parentDistrict) :
    aabb(aabb), plotIndex(plotIndex), parentDistrict(parentDistrict) {
};

CityPlot::~CityPlot()
{

}
