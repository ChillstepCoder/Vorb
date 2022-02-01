#include "stdafx.h"
#include "CityPlot.h"

#include "BuildingBlueprint.h"

CityPlot::CityPlot()
{

}

CityPlot::CityPlot(const ui32AABB2& aabb, CityPlotIndex plotIndex, CityDistrict* parentDistrict) :
    aabb(aabb), plotIndex(plotIndex), parentDistrict(parentDistrict) {
};

CityPlot::~CityPlot()
{

}
