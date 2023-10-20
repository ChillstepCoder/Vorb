#pragma once

#include <Vorb/Event.hpp>

enum class EDITOR_SELECTION_EVENT_TYPE {
    SelectionChanged,
};
struct EditorSelectionEvent {
    UniqueId64 selectionID;
    EditorSelectionContext context;
    bool didSelect;
};
EVENT_DISPATCHER_TYPE(EditorSelectionManager, EDITOR_SELECTION_EVENT_TYPE, EditorSelectionEvent&);

enum class EditorSelectionContext {
    Global = 0, ContentBrowser
};

// Static - based on Hazel
class EditorSelectionManager {
public:
    static void select(EditorSelectionContext context, UniqueId64 selectionID);
    static bool isSelected(UniqueId64 selectionID);
    static bool isSelected(EditorSelectionContext context, UniqueId64 selectionID);
    static void deselect(UniqueId64 selectionID);
    static void deselect(EditorSelectionContext context, UniqueId64 selectionID);
    static void deselectAll();
    static void deselectAll(EditorSelectionContext context);
    static UniqueId64 getSelection(EditorSelectionContext context, size_t index);

    static size_t getSelectionCount(EditorSelectionContext contextID);
    inline static const std::vector<UniqueId64>& getSelections(EditorSelectionContext context) { return sContexts[context]; }

    STATIC_EVENT_LISTENER_FUNCS(EditorSelectionManager, SelectionEvent, EDITOR_SELECTION_EVENT_TYPE::SelectionChanged, EditorSelectionEvent&);

private:
    inline static std::unordered_map<EditorSelectionContext, std::vector<UniqueId64>> sContexts;

    STATIC_EVENT_DISPATCHER_DEF(EditorSelectionManager);
};

