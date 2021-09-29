#pragma once

class CityPlanner;
class CityBuilder;
class CityPlotter;
class CityQuartermaster;

class Camera2D;

class CityDebugRenderer
{
public:
    void renderCityPlannerDebug(const CityPlanner& cityPlanner) const;
    void renderCityBuilderDebug(const CityBuilder& cityBuilder) const;
    void renderCityPlotterDebug(const CityPlotter& cityPotter) const;
    void renderCityQuartermasterDebug(const CityQuartermaster& cityPotter) const;

    void finishRenderFrame();
    void clearMeshes();

    bool mNeedsMeshes = true;
};

