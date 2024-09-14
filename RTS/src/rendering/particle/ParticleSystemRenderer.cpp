#include "stdafx.h"
#include "ParticleSystemRenderer.h"

#include "rendering/MaterialRenderer.h"
#include "rendering/MaterialShaderRepository.h"
#include "camera/Camera3D.h"

#include "rendering/particle/CpuParticleEmitter.h"

#include <Vorb/graphics/DepthState.h>

void ParticleSystemRenderer::renderEmitters(CpuParticleEmitterRenderList& renderList, const Camera3D& camera) {
    PROFILE_FUNCTION();
    ASSERT_RENDER_THREAD();

    glDisable(GL_CULL_FACE);
    for (int blendMode = 0; blendMode < e_count(ParticleBlendMode); ++blendMode) {
        FlatMap<AssetID /*materialShader*/, std::vector<EmitterRenderData>>& emitters = renderList[blendMode];
        if (emitters.empty()) {
            continue;
        }
        bindStateForParticleBlendMode((ParticleBlendMode)blendMode);

        static_assert(e_count(ParticleBlendMode) == 4, "Update switch statement for new ParticleBlendMode enum values");
        for (auto& [shaderID, emitterList] : emitters) {
            const MaterialShaderDef* shader = MaterialShaderRepository::get().tryGetLoadedAsset(shaderID);
            if (!shader) {
                continue;
            }
            VGUniform unRootPos = shader->getUniform("unRootPos");
            MaterialRenderer::bindMaterialShaderForRender(*shader);
            glUniformMatrix4fv(shader->getUniform("unVP"), 1, false, &camera.getVPMatrix()[0][0]);

            const VGUniform* lifetimeUniform = shader->tryGetUniform("EmitterNormalizedLifetime");
            for (auto& renderData : emitterList) {
                glUniform3fv(unRootPos, 1, (const GLfloat*)renderData.rootPosition);

                if (lifetimeUniform) {
                    glUniform1f(*lifetimeUniform, renderData.emitter->getNormalizedLifetime());
                }

                renderData.emitter->render();
            }
        }

        emitters.clear();
    }
}

void ParticleSystemRenderer::renderEmitterEditor(CpuParticleEmitter* emitter, const f32m4& VP, f32v3 rootPosition) {
    const MaterialShaderDef* shader = MaterialShaderRepository::get().tryGetLoadedAsset(emitter->getShaderID());
    if (!shader) {
        return;
    }
    VGUniform unRootPos = shader->getUniform("unRootPos");
    MaterialRenderer::bindMaterialShaderForRender(*shader);
    glUniformMatrix4fv(shader->getUniform("unVP"), 1, false, &VP[0][0]);

    const VGUniform* lifetimeUniform = shader->tryGetUniform("EmitterNormalizedLifetime");
    glUniform3fv(unRootPos, 1, &rootPosition.x);

    if (lifetimeUniform) {
        glUniform1f(*lifetimeUniform, emitter->getNormalizedLifetime());
    }

    emitter->render();
}

void ParticleSystemRenderer::bindStateForParticleBlendMode(ParticleBlendMode blendMode) {

    // TODO: Switch to using RenderDevice
    switch (blendMode) {
        case ParticleBlendMode::Opaque:
            vg::DepthState::FULL.set();
            vg::sBlendStates.REPLACE.set();
            break;
        case ParticleBlendMode::Alpha:
            vg::DepthState::READ.set();
            vg::sBlendStates.ALPHA.set();
            break;
        case ParticleBlendMode::Additive:
            vg::DepthState::READ.set();
            vg::sBlendStates.ADDITIVE.set();
            break;
        case ParticleBlendMode::Subtractive:
            vg::DepthState::READ.set();
            vg::sBlendStates.SUBTRACTIVE.set();
            break;
    }
}
