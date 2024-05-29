#include "stdafx.h"
#include "HostEffectContext.h"

void HostEffectContext::playParticleEffectAtPoint(EffectAssetRef effectName, f32v3 point, ParticleSystemInputs inputs, BitFlags<EffectCreateFlags> flags) {
    mCliContext.playParticleEffectAtPoint(effectName, point, inputs, flags);

    // TODO: Replicate to other clients
    //if (flags.isBitSet(EffectCreateFlags::REPLICATE) && GameServer::exists()) {
    //    mRegistry.emplace<ReplicationComponent>(newEntity);
    //    SrvMessage::sendEntityCreateMessageToAll(newEntity, typeToken, position, 0.0f);
    //    // Details are only used for replication right now
    //    EntityDetailsComponent& detailsCmp = mRegistry.emplace<EntityDetailsComponent>(newEntity);
    //    detailsCmp.mEntityToken = typeToken;
    //}
}
