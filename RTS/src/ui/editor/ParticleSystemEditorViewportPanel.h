#pragma once
#include "IEditorViewportPanel.h"

#include "resources/IAssetRepository.h"
#include "definitions/ParticleSystemDef.h"

class ParticleEmitterDef;
class CPUParticleEmitterModule;
class CPUParticleSystem;

class ParticleSystemEditorViewportPanel : public IEditorViewportPanel {
public:
    ParticleSystemEditorViewportPanel();
    ~ParticleSystemEditorViewportPanel();

    bool updateAndRender(f32 elapsedSec) override;
    void updateAndRenderPrimaryControls(f32 ySize) override;
    bool updateAndRenderSecondaryControls(f32 ySize) override;
    bool updateAndRenderTertiaryControls(f32 ySize) override;
    bool hasBottomControls() const override { return true; }
    f32 getBottomHeight() const override { return mBottomHeight; }
    void updateAndRenderBottomControls() override;

    void renderMesh() override;

    void setParticleSystemDef(AssetID systemId);

private:
    void createPreviewSystem();
    void updatePopups();

    void openDuplicateEmitterPopup();
    void duplicateGlobalEmitter(const nString& emitterName);

    bool systemIsLoaded() const { return mSystemDefHandle != nullptr && mSystemDefHandle->isLoaded(); }

    AssetHandlePtr<ParticleSystemDef> mSystemDefHandle = nullptr;
    ParticleSystemDef* mSystemDef = nullptr;

    ParticleEmitterDef* mSelectedEmitter = nullptr;
    CPUParticleEmitterModule* mSelectedModule = nullptr;
    std::vector<bool> mShowEmitters; // One for each emitter in the system

    // Assets requested to load
    AssetHandleBundle mAssetHandleBundle;

    //Popups
    std::unique_ptr<ImguiUtil::RenameAssetPopup> mRenamePopup;
    std::unique_ptr<ImguiUtil::ConfirmDeletePopup> mConfirmDeletePopup;
    std::unique_ptr<ImguiUtil::CustomSelectorPopup> mDuplicateObjectPopup;

    std::unique_ptr<CPUParticleSystem> mPreviewSystem;
    f32 mTimelineEnd = 3.0f;
    f32 mCurrentTime = 0.0f;
    f32 mCurrentElapsedSec = 0.0f;

    f32 mBottomHeight = 120.0f;

    static constexpr size_t TEXT_INPUT_SIZE = 64;
    char mTextInputBuffer[TEXT_INPUT_SIZE];

    bool mIsNewSystemPopupOpen = false;
};

