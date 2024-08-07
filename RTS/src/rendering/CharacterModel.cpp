#include "stdafx.h"
#include "CharacterModel.h"

void CharacterLinkedSubmodelComponent::addLinkedSubmodel(entt::entity thisEntity, LinkedSubmodel linkedSubmodel) {
    linkedSubmodels.emplace_back(linkedSubmodel);
    CharacterModelEvents::dispatchSubmodelAdded({ thisEntity, linkedSubmodel });
}

void CharacterLinkedSubmodelComponent::removeLinkedSubmodel(entt::entity thisEntity, LinkedSubmodel linkedSubmodel) {
    for (size_t i = 0; i < linkedSubmodels.size(); ++i) {
        if (linkedSubmodels[i] == linkedSubmodel) {
            linkedSubmodels[i] = std::move(linkedSubmodels[linkedSubmodels.size() - 1]);
            linkedSubmodels.pop_back();
            CharacterModelEvents::dispatchSubmodelRemoved({ thisEntity, linkedSubmodel });
            return;
        }
    }
    auto it = std::find(linkedSubmodels.begin(), linkedSubmodels.end(), linkedSubmodel);
    if (it != linkedSubmodels.end()) {
        linkedSubmodels.erase(it);
        CharacterModelEvents::dispatchSubmodelRemoved({ thisEntity, linkedSubmodel });
    }
}

void CharacterLinkedSubmodelComponent::toggleLinkedSubmodel(entt::entity thisEntity, LinkedSubmodel linkedSubmodel) {
    for (size_t i = 0; i < linkedSubmodels.size(); ++i) {
        if (linkedSubmodels[i] == linkedSubmodel) {
            linkedSubmodels[i] = std::move(linkedSubmodels[linkedSubmodels.size() - 1]);
            linkedSubmodels.pop_back();
            CharacterModelEvents::dispatchSubmodelRemoved({ thisEntity, linkedSubmodel });
            return;
        }
    }
    linkedSubmodels.emplace_back(linkedSubmodel);
    CharacterModelEvents::dispatchSubmodelAdded({ thisEntity, linkedSubmodel });
}

bool CharacterLinkedSubmodelComponent::hasLinkedSubmodel(LinkedSubmodel submodel) {
    return std::find(linkedSubmodels.begin(), linkedSubmodels.end(), submodel) != linkedSubmodels.end();
}
