#include "stdafx.h"
#include "RigDef.h"


void RigDef::imguiRenderSkeletonHierarchy() const {
    ImGui::Text("Num Joints %d", mSkeleton.num_joints());

    // Lazy init
    if (!mJointEditorNodes) [[unlikely]] {
        mJointEditorNodes = std::make_unique<JointEditorNode[]>(mSkeleton.num_joints());
        for (ui32 i = 0; i < mSkeleton.num_joints(); ++i) {
            int parent = mSkeleton.joint_parents()[i];
            if (parent != -1) {
                mJointEditorNodes[parent].childNodes.emplace_back(i);
            }
            else {
                mEditorRootNode = i;
            }
            mJointEditorNodes[i].name = mSkeleton.joint_names()[i];
        }
    }
    // Recursive lambda
    std::function<void(ui32 i)> recursiveImguiBoneHeirarchy;
    recursiveImguiBoneHeirarchy = [&recursiveImguiBoneHeirarchy, this](ui32 i) {
        if (ImGui::TreeNodeEx(mJointEditorNodes[i].name, ImGuiTreeNodeFlags_DefaultOpen, "%s %d", mJointEditorNodes[i].name, i)) {
            for (ui32 child : mJointEditorNodes[i].childNodes) {
                recursiveImguiBoneHeirarchy(child);
            }
            ImGui::TreePop();
        }
    };

    recursiveImguiBoneHeirarchy(mEditorRootNode);
}
