#pragma once

#include <variant>
struct ModelDef;
struct MaterialData;

enum class TileEditorPanelResultCode {
    NONE,
    EDIT_MODEL,
    EDIT_MATERIAL,
    COUNT
};

typedef std::variant<ModelDef*, MaterialData*> TileEditorPanelResultVariant;

typedef std::pair<TileEditorPanelResultCode, TileEditorPanelResultVariant> TileEditorPanelResult;

class TileEditorPanel
{
public:
    TileEditorPanelResult updateAndRender(float ySize);
};

