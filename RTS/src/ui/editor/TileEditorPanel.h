#pragma once

#include <variant>
struct ModelDef;

enum class TileEditorPanelResultCode {
    NONE,
    EDIT_MODEL,
    COUNT
};

typedef std::variant<ModelDef*> TileEditorPanelResultVariant;

typedef std::pair<TileEditorPanelResultCode, TileEditorPanelResultVariant> TileEditorPanelResult;

class TileEditorPanel
{
public:
    TileEditorPanelResult updateAndRender(float ySize);
};

