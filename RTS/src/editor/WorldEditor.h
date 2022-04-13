#pragma once

class World;
class Camera3D;

#include "world/ChunkID.h"
#include "util/IntersectionHit.h"

class Brush;
class BrushRepository;

enum class WorldEditorEditMode {
    TERRAIN,
    GRASS,
    TILE,
    ENTITY,
    CITY,
    BUILDING,
    COUNT
};

enum class TerrainEditState {
    RAISE_TERRAIN,
    LOWER_TERRAIN,
    FLATTEN_TERRAIN,
    COUNT
};

enum class GrassEditState {
    ADD,
    REMOVE,
    COUNT
};

enum class CityEditState {
    NONE,
    CREATE,
    COUNT
};

enum class BuildingEditState {
    NONE,
    CREATE,
    DESTROY,
    COUNT
};

struct BrushSettings {
    const Brush* activeBrush;
    ui32 brushId;
    f32 brushSize;
    f32 brushStrength;
};

class WorldEditor {
public:
    WorldEditor(World& world, const f32v2& screenDims);

    void update(const Camera3D& camera);

	void renderBrushDecals(const Camera3D& camera) const;
    void renderUI() const;

private:
    void renderModeButtons() const;
    void tryRenderBrushSelect(const BrushRepository& brushRepo) const;
    void renderTerrainEditUI() const;
    void renderGrassEditUI() const;
    void renderTileEditUI() const;
    void renderEntityEditUI() const;
    void renderCityEditUI() const;
    void renderBuildingEditUI() const;

    void updateTerrainEdit();
    void updateGrassEdit();
    void updateTileEdit();
    void updateEntityEdit();
    void updateCityEdit();
    void updateBuildingEdit();

    void editVertex(ChunkID id, const ui32v2& vertPos, const f32v2& offsetToVertex);
    void editGrass(ChunkID id, TileIndex tileIndex, const f32v2& offsetToTile);
    f32 getBrushStrengthAtPoint(const f32v2& brushOffsetToPoint);
    void setEditMode(WorldEditorEditMode mode) const;

    World& mWorld;
    f32v2 mScreenDims;

    // Edit states
    mutable WorldEditorEditMode mEditMode = WorldEditorEditMode::TERRAIN;
    mutable TerrainEditState mTerrainEditState = TerrainEditState::RAISE_TERRAIN;
    mutable GrassEditState mGrassEditState = GrassEditState::ADD;
    mutable CityEditState mCityEditState = CityEditState::CREATE;
    mutable BuildingEditState mBuildingEditState = BuildingEditState::CREATE;
    // Brushes
    mutable BrushSettings mTerrainBrushSettings = { nullptr, UINT32_MAX, 5.0f, 0.1f };
    mutable BrushSettings mGrassBrushSettings = { nullptr, UINT32_MAX, 5.0f, 1.0f };
    mutable BrushSettings* mCurrentBrushSettings = &mTerrainBrushSettings;
    // Tile Edit
    mutable ui32 mSelectedTile = 0;
    mutable f32 mGroundTileOffset = 1.0f;
    // Building Edit
    mutable ui32 mSelectedBuilding = 0;
    mutable i32v2 mPlotDims = i32v2(16);

    mutable nString mSelectedEntity = "";
    TerrainPickData mPickData;
};

