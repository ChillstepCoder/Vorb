#pragma once

#include "debugging/SimpleMesh.h"

struct SimpleLine {
    f32v3 position1;
    f32v3 position2;
};

struct SimpleQuad {
    f32v3 position;
    f32v2 dims;
};

enum class VisualLogShapeType {
    LINE,
    WIRE_QUAD,
    QUAD,
    ARROW,
};

struct VisualLogShape {
    union {
        SimpleLine line;
        SimpleQuad quad;
    };
    color4 color;
    VisualLogShapeType type;
};

struct VisualLogRenderStep {
    std::vector<VisualLogShape> mShapes;
};

struct VisualLogRenderStepInfo {
    ui32 startIndex;
    ui32 shapeCount;
    nString stepName;
};

class VisualLog {
    friend class VisualLogger;
public:
    VisualLog(const nString& name);
    ~VisualLog();

    VORB_NON_COPYABLE_BUT_MOVABLE(VisualLog);

    // TODO: Pool allocator?

    void setRootPos(const f32v3& rootPos) { mRootPos = rootPos; }
    void reserve(ui32 shapeCount);
    void nextStep(const nString& stepName);

    void addLineBetweenPoints(const f32v3& origin, const f32v3& end, const color4& color);
    void addWireQuad(const f32v3& origin, const f32v2& dims, color4 color);
    void addFilledQuad(const f32v3& origin, const f32v2& dims, color4 color);
    void addCartesianArrow(const f32v3& center, f32 length, color4 color, Cartesian dir);

    void finish();

    void render(const f32v3& cameraPos, const f32m4& viewMatrix);

private:
    void buildMesh();

    std::vector<VisualLogRenderStepInfo> mRenderStepInfo;
    std::vector<VisualLogShape> mShapes;
    std::atomic_bool mFinishedBuilding = false;
    std::atomic_bool mShouldRender = false;

    // Drawing
    int mSelectedRenderStep = 0;
    int mShapesToRender = 0;
    bool mRenderSingleStep = true;
    bool mRenderSingleShape = false;
    bool mDirtyRender = true;

    f32v3 mRootPos = f32v3(0.0f);
    ui32 mNumQuads = 0;
    ui32 mNumLines = 0;
    ui32 mNumArrows = 0;
    SimpleMesh mLinesMesh;
    SimpleMesh mQuadsMesh;
    nString mName;
};

class VisualLogger {
public:
    static VisualLog* tryGetNewVisualLog(const nString& name);
    static void renderImgui();

    static void renderActiveLogs(const f32v3& cameraPos, const f32m4& viewMatrix);
    
    static std::vector<std::unique_ptr<VisualLog>> sVisualLogs;

private:
    static void deleteLog(VisualLog* log);
    static std::mutex sMutex;
};
