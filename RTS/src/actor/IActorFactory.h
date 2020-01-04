#pragma once
#include "stdafx.h"
#include <Vorb/ecs/Entity.h>

DECL_VECS(class ECS);
DECL_VIO(class Path);

class IActorFactory {
public:
	IActorFactory(vecs::ECS& ecs) : mEcs(ecs) { };
	virtual ~IActorFactory() = default;

	// TODO??
	//virtual vecs::EntityID createActor(const f32v2& position, VGTexture texture, const vio::Path& definitionFile) = 0;

protected:

	vecs::ECS& mEcs;
};
