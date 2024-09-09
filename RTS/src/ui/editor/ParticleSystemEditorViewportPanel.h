#pragma once
#include "AssetEditorViewportPanel.h"

#include "resources/IAssetRepository.h"
#include "definitions/ParticleSystemDef.h"

class ParticleEmitterDef;
class CPUParticleEmitterModule;
class CPUParticleSystem;

class ParticleSystemEditorViewportPanel : public AssetEditorViewportPanel<ParticleSystemDef> {
public:
    ParticleSystemEditorViewportPanel();
    ~ParticleSystemEditorViewportPanel();

    void updateAndRenderPrimaryControls(f32 ySize) override;
    bool updateAndRenderSecondaryControls(f32 ySize) override;
    bool updateAndRenderTertiaryControls(f32 ySize) override;
    bool hasBottomControls() const override { return true; }
    void updateAndRenderBottomControls() override;

    void renderMesh() override;

    const char* getViewportWindowName() const override { return "Particle System Editor"; }

private:
    void updateAndRenderInternal(f32 elapsedSec) override;
    void createPreviewSystem();
    void updatePopups();

    void openDuplicateEmitterPopup();
    void duplicateGlobalEmitter(const nString& emitterName);

    std::vector<nString> getGlobalEmitterNames();

    bool systemIsLoaded() const { return mAssetHandle != nullptr && mAssetHandle->isLoaded(); }

    void unselect();

    ParticleEmitterDef* mSelectedEmitter = nullptr;
    CPUParticleEmitterModule* mSelectedModule = nullptr;
    CPUParticleEmitterModuleVector* mSelectedModuleVector = nullptr;
    std::vector<bool> mShowEmitters; // One for each emitter in the system

    // Assets requested to load
    AssetHandleBundle mAssetHandleBundle;

    //Popups
    std::unique_ptr<ImguiUtil::RenameAssetPopup> mRenamePopup;
    std::unique_ptr<ImguiUtil::ConfirmDeletePopup> mConfirmDeletePopup;
    std::unique_ptr<ImguiUtil::CustomSelectorPopup> mDuplicateObjectPopup;
    std::unique_ptr<ImguiUtil::AssetSelectorPopup> mAssetSelectorPopup;

    std::unique_ptr<CPUParticleSystem> mPreviewSystem;
    f32 mTimelineEnd = 3.0f;
    f32 mCurrentTime = 0.0f;
    f32 mUpdateTimeMs = 0.0f;

    f32 mBottomHeight = 120.0f;
    ui32 mNumParticles = 0;

    static constexpr size_t TEXT_INPUT_SIZE = 64;
    char mTextInputBuffer[TEXT_INPUT_SIZE];

    bool mIsNewSystemPopupOpen = false;
};

