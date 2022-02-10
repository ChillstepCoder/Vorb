#pragma once

class CityPlanner;
class CityBuilder;
class CityPlotter;
class CityQuartermaster;
struct BuildingBlueprint;

class Camera2D;

class CityDebugRenderer
{
public:
    void renderCityPlannerDebug(const CityPlanner& cityPlanner) const;
    void renderCityBuilderDebug(const CityBuilder& cityBuilder) const;
    void renderCityPlotterDebug(const CityPlotter& cityPlotter) const;
    void renderCityQuartermasterDebug(const CityQuartermaster& cityQuartermaster) const;

    void finishRenderFrame();
    void clearMeshes();

    static void renderBlueprintDebug(BuildingBlueprint& bp, color4* inputColor = nullptr);

    bool mNeedsMeshes = true;
};

