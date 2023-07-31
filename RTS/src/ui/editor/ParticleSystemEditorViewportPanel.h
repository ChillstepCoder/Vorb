#pragma once
#include "IEditorViewportPanel.h"

struct ParticleSystemDef;
struct ParticleEmitterDef;
class CPUParticleEmitterModule;

class ParticleSystemEditorViewportPanel : public IEditorViewportPanel {
public:
    bool updateAndRender() override;
    void updateAndRenderPrimaryControls(f32 ySize) override;
    bool updateAndRenderSecondaryControls(f32 ySize) override;
    bool updateAndRenderTertiaryControls(f32 ySize) override;

    void renderMesh() override;

    void setParticleSystemDef(ParticleSystemDef* systemDef);

private:

    ParticleSystemDef* mSystemDef = nullptr;
    ParticleEmitterDef* mSelectedEmitter = nullptr;
    const CPUParticleEmitterModule* mSelectedModule = nullptr;

    static constexpr size_t TEXT_INPUT_SIZE = 64;
    char mTextInputBuffer[TEXT_INPUT_SIZE];

    bool mIsNewSystemPopupOpen = false;
};

