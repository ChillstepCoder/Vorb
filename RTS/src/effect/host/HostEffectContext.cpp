#include "stdafx.h"
#include "HostEffectContext.h"

void HostEffectContext::playParticleEffectAtPoint(
    EffectAssetRef effectName, f32v3 point, f32q orientation, ParticleSystemInputsPtr inputs, BitFlags<EffectCreateFlags> flags
) {

    // TODO: Replicate to other clients
    //if (flags.isBitSet(EffectCreateFlags::REPLICATE) && GameServer::exists()) {
    //    mRegistry.emplace<ReplicationComponent>(newEntity);
    //    SrvMessage::sendEntityCreateMessageToAll(newEntity, typeToken, position, 0.0f);
    //    // Details are only used for replication right now
    //    EntityDetailsComponent& detailsCmp = mRegistry.emplace<EntityDetailsComponent>(newEntity);
    //    detailsCmp.mEntityToken = typeToken;
    //}

    mCliContext.playParticleEffectAtPoint(effectName, point, orientation, std::move(inputs), flags);
}

void HostEffectContext::playMutationEffect(
    const f32m4& transform, ModelID startModel, ModelID endModel, TileMutationType mutationType, BitFlags<EffectCreateFlags> flags
) {

    // TODO: Replicate to other clients
    //if (flags.isBitSet(EffectCreateFlags::REPLICATE) && GameServer::exists()) {

    mCliContext.playMutationEffect(transform, startModel, endModel, mutationType, flags);
}