#pragma once


#include "world/GridID.h"
#include "physics/PhysHitResult.h"
#include "tile/TileGrass.h"

#include "definitions/BrushDef.h"

class Camera3D;
class BrushRepository;
class World;

enum class WorldEditorEditMode {
    TERRAIN,
    ROAD,
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
    DIRTY_ONLY,
    COUNT
};

enum class RoadEditState {
    ADD,
    REMOVE
};

enum class GrassEditState {
    RAISE,
    LOWER,
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
    AssetHandlePtr<BrushDef> activeBrush;
    AssetID brushId;
    f32 brushSize;
    f32 brushStrength;
};

constexpr f64 WORLD_EDITOR_UPDATE_RATE_MS = 32.0;

class WorldEditorPanel {
public:
    WorldEditorPanel();

    void update(World* world, const Camera3D& camera, const f32v3& pickRay);

	void renderBrushDecals(const Camera3D& camera) const;
    void renderUI(f32 ySize) const;

    World* getActiveWorld() const { return mActiveWorld; }

private:
    void renderMenuBar() const;
    void tryRenderBrushSelect() const;
    void renderTerrainEditUI() const;
    void renderRoadEditUI() const;
    void renderGrassEditUI() const;
    void renderTileEditUI() const;
    void renderEntityEditUI() const;
    void renderCityEditUI() const;
    void renderBuildingEditUI() const;

    void updateTerrainEdit();
    void updateRoadEdit();
    void updateGrassEdit();
    void updateTileEdit();
    void updateEntityEdit();
    void updateCityEdit();
    void updateBuildingEdit();

    void editHeightVertex(HeightmapPatchID id, DTileCoord vertPos, f32v2 offsetToVertex, const BrushSettings& brush, TerrainEditState editState);
    void editRoadVertex(i32v2 worldPos, f32v2 offsetToVertex, const BrushSettings& brush, RoadEditState editState);
    void editGrass(ChunkID id, TileIndex tileIndex, TileGrassID grassId, const f32v2& offsetToTile, const BrushSettings& brush, GrassEditState editState);
    f32 getBrushStrengthAtPoint(const BrushSettings& brush, const f32v2& brushOffsetToPoint);
    void setEditMode(WorldEditorEditMode mode) const;

    // Edit states
    mutable WorldEditorEditMode mEditMode = WorldEditorEditMode::TERRAIN;
    mutable TerrainEditState mTerrainEditState = TerrainEditState::RAISE_TERRAIN;
    mutable GrassEditState mGrassEditState = GrassEditState::RAISE;
    mutable CityEditState mCityEditState = CityEditState::CREATE;
    mutable BuildingEditState mBuildingEditState = BuildingEditState::CREATE;
    mutable RoadEditState mRoadEditState = RoadEditState::ADD;
    // Brushes
    mutable BrushSettings mTerrainBrushSettings = { nullptr, UINT32_MAX, 5.0f, 0.1f };
    mutable BrushSettings mRoadBrushSettings = { nullptr, UINT32_MAX, 5.0f, 1.0f };
    mutable BrushSettings mGrassBrushSettings = { nullptr, UINT32_MAX, 5.0f, 1.0f };
    mutable BrushSettings* mCurrentBrushSettings = &mTerrainBrushSettings;
    // Tile Edit
    mutable ui32 mSelectedTile = 0;
    mutable int mSelectedFloor = 0;
    mutable f32 mGroundTileOffset = 1.0f;
    mutable bool mDragToPlace = false;
    mutable bool mDidPlaceTile = false;
    // Building Edit
    mutable ui32 mSelectedBuilding = 0;
    mutable i32v2 mPlotDims = i32v2(16);
    // Grass edit
    mutable TileGrassID mSelectedGrass = 0;

    mutable StrToken mSelectedEntity;
    std::unique_ptr<DeferredPhysicsPick> mDeferredPhysicsPick;
    PhysHitResult mHitResult;
    TickingTimer mUpdateTimer = TickingTimer(WORLD_EDITOR_UPDATE_RATE_MS);

    World* mActiveWorld = nullptr;
};

