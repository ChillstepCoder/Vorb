#include "stdafx.h"
#include "EditorSelectionManager.h"

void EditorSelectionManager::select(EditorSelectionContext contextID, UniqueId64 selectionID) {
    auto& contextSelections = sContexts[contextID];
    if (std::find(contextSelections.begin(), contextSelections.end(), selectionID) != contextSelections.end())
        return;

    // TODO: Maybe verify if the selectionID is already selected in another context?
    contextSelections.push_back(selectionID);

    EditorSelectionEvent evnt;
    evnt.context = contextID;
    evnt.selectionID = selectionID;
    evnt.didSelect = true;
    dispatchSelectionEvent(evnt);
}

bool EditorSelectionManager::isSelected(UniqueId64 selectionID) {
    for (const auto& [contextID, contextSelections] : sContexts) {
        if (std::find(contextSelections.begin(), contextSelections.end(), selectionID) != contextSelections.end()) {
            return true;
        }
    }

    return false;
}

bool EditorSelectionManager::isSelected(EditorSelectionContext contextID, UniqueId64 selectionID) {
    const auto& contextSelections = sContexts[contextID];
    return std::find(contextSelections.begin(), contextSelections.end(), selectionID) != contextSelections.end();
}

void EditorSelectionManager::deselect(UniqueId64 selectionID)
{
    for (auto& [contextID, contextSelections] : sContexts)
    {
        auto it = std::find(contextSelections.begin(), contextSelections.end(), selectionID);
        if (it == contextSelections.end())
            continue;

        EditorSelectionEvent evnt;
        evnt.context = contextID;
        evnt.selectionID = selectionID;
        evnt.didSelect = false;
        dispatchSelectionEvent(evnt);
        break;
    }
}

void EditorSelectionManager::deselect(EditorSelectionContext contextID, UniqueId64 selectionID) {
    auto& contextSelections = sContexts[contextID];
    auto it = std::find(contextSelections.begin(), contextSelections.end(), selectionID);
    if (it == contextSelections.end())
        return;

    contextSelections.erase(it);
}

void EditorSelectionManager::deselectAll() {
    for (auto& [ctxID, contextSelections] : sContexts) {
        for (const auto& selectionID : contextSelections) {
            EditorSelectionEvent evnt;
            evnt.context = ctxID;
            evnt.selectionID = selectionID;
            evnt.didSelect = false;
            dispatchSelectionEvent(evnt);
        }
        contextSelections.clear();
    }
}

void EditorSelectionManager::deselectAll(EditorSelectionContext contextID)
{
    auto& contextSelections = sContexts[contextID];

    for (const auto& selectionID : contextSelections) {
        EditorSelectionEvent evnt;
        evnt.context = contextID;
        evnt.selectionID = selectionID;
        evnt.didSelect = false;
        dispatchSelectionEvent(evnt);
    }

    contextSelections.clear();
}

UniqueId64 EditorSelectionManager::getSelection(EditorSelectionContext context, size_t index) {
    auto& contextSelections = sContexts[context];
    assert(index >= 0 && index < contextSelections.size());
    return contextSelections[index];
}

size_t EditorSelectionManager::getSelectionCount(EditorSelectionContext contextID) {
    return sContexts[contextID].size();
}
