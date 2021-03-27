#pragma once

class CityPlanner;
class CityBuilder;
class CityPlotter;

class Camera2D;

class CityDebugRenderer
{
public:
    void renderCityPlannerDebug(const CityPlanner& cityPlanner) const;
    void renderCityBuilderDebug(const CityBuilder& cityBuilder) const;
    void renderCityPlotterDebug(const CityPlotter& cityPotter) const;
};

