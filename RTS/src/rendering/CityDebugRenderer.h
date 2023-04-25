#pragma once

class CityPlanner;
class CityBuilder;
class CityPlotter;
class CityQuartermaster;
class BuildingBlueprint;

class CityDebugRenderer
{
public:
    void renderCityPlannerDebug(const CityPlanner& cityPlanner) const;
    void renderCityBuilderDebug(const CityBuilder& cityBuilder) const;
    void renderCityPlotterDebug(const CityPlotter& cityPlotter) const;
    void renderCityQuartermasterDebug(const CityQuartermaster& cityQuartermaster) const;

    void finishRenderFrame();
    void clearMeshes();

    static void renderBlueprintDebug(BuildingBlueprint& bp, int lifetime, color4* inputColor = nullptr);

    bool mNeedsMeshes = true;
};

