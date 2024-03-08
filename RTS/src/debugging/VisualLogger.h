#pragma once

#include "rendering/mesh/Mesh.h"
#include "debugging/SimpleMesh.h"

struct Font;

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
    TEXT,
    COUNT
};

struct VisualLogShape {
    union {
        SimpleLine line;
        SimpleQuad quad;
        ui32 textIndex;
    };
    color4 color;
    VisualLogShapeType type;
};

struct VisualLogRenderStep {
    std::vector<VisualLogShape> mShapes;
};

struct VisualLogRenderStepInfo {

    void end() { totalMs = (f32)timer.stop(); }

    nString stepName;
    ui32 startIndex;
    ui32 shapeCount;
    PreciseTimer timer;
    f32 totalMs;
};

struct VisualLogTextData {
    const Font* font;
    nString str;
    f32v3 rootPos;
    f32v2 offset2D;
    f32 glyphHeight;
};

enum class VisualLogCategory : ui8 {
    Building,
    Settlement,
    COUNT
};
SERIALIZABLE_ENUM_SAME_NAME(VisualLogCategory,
    ENUM_FIELD_SIMPLE(VisualLogCategory, Building),
    ENUM_FIELD_SIMPLE(VisualLogCategory, Settlement)
);

class VisualLog {
    friend class VisualLogger;
public:
    VisualLog(const nString& name, VisualLogCategory category, bool isHiddenInUI);
    ~VisualLog();

    VORB_NON_COPYABLE_BUT_MOVABLE(VisualLog);

    // TODO: Pool allocator?

    void setRootPos(const f32v3& rootPos) { mRootPos = rootPos; }
    // Allows you to have a fake root position when using world space coordinates for the log. Added to RootPos (typically 0)
    void setCameraDistanceCheckPosOffset(const f32v3& posOffset) { mCameraDistanceCheckPosOffset = posOffset; }
    void reserve(ui32 shapeCount);
    void nextStep(const nString& stepName);

    void addLineBetweenPoints(const f32v3& origin, const f32v3& end, const color4& color);
    void addWireQuad(const f32v3& origin, const f32v2& dims, color4 color);
    void addWireTriangle(const f32v3 points[3], color4 color);
    void addFilledQuad(const f32v3& origin, const f32v2& dims, color4 color);
    void addCartesianArrow(const f32v3& center, f32 length, color4 color, Cartesian dir);
    void addText(const nString& str, const f32v3& rootPosition, const Font& font, f32 glyphHeight, const f32v2& offset2D, color4 color);
    void addText(const nString& str, const f32v3& rootPosition, f32 glyphHeight, const f32v2& offset2D, color4 color);

    void finish();

    void render(const f32v3 cameraPos, const f32m4& viewMatrix);

    void setUserString(const nString& userString) { mUserString = userString; }
    void setIsHiddenInUI(bool isHiddenInUI) { mIsHiddenInUI = isHiddenInUI; }

private:
    void buildMesh();

    std::vector<VisualLogRenderStepInfo> mRenderStepInfo;
    std::vector<VisualLogShape> mShapes;
    std::vector<VisualLogTextData> mTextData;
    std::atomic_bool mFinishedBuilding = false;
    std::atomic_bool mShouldRender = false;

    // Drawing
    int mSelectedRenderStep = 0;
    int mShapesToRender = 0;
    bool mRenderSingleStep = true;
    bool mRenderSingleShape = false;
    bool mDirtyRender = true;
    bool mIsHiddenInUI = false;

    f32v3 mRootPos = f32v3(0.0f);
    f32v3 mCameraDistanceCheckPosOffset = f32v3(0.0f);
    ui32 mNumQuads = 0;
    ui32 mNumLines = 0;
    ui32 mNumArrows = 0;
    SimpleMesh mLinesMesh;
    SimpleMesh mQuadsMesh;
    Mesh mTextMesh;
    nString mName;
    nString mUserString;
    VisualLogCategory mCategory;

    inline static const Font* sDefaultFont = nullptr;
};

class VisualLogger {
public:
    static VisualLog* tryGetNewVisualLog(const nString& name, VisualLogCategory category, bool isHidden);
    static void renderImgui(f32v3 cameraPos);

    static void renderActiveLogs(const f32v3 cameraPos, const f32m4& viewMatrix);
    
    static std::map<VisualLogCategory, std::vector<std::unique_ptr<VisualLog>>> sVisualLogs;
    static std::vector<bool> sFirstCategoryOpen; // For unhiding first log in category

    static void setDefaultFont(const Font* font) { VisualLog::sDefaultFont = font; }
    static void unHideClosestLogInCategory(f32v3 cameraPos, VisualLogCategory category);

private:
    static void deleteLog(VisualLog* log);
    static std::mutex sMutex;
};
