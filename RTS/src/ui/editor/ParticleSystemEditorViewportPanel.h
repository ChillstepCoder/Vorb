#pragma once
#include "IEditorViewportPanel.h"

class ParticleSystemDef;

class ParticleSystemEditorViewportPanel : public IEditorViewportPanel {
public:
    bool updateAndRender() override;
    void updateAndRenderControls(f32 ySize) override;

    void renderMesh() override;

    void setParticleSystemDef(ParticleSystemDef& fishDef);

private:
    const MaterialShader* getShader() override;

    ParticleSystemDef* mSystemDef = nullptr;
};

