#include "stdafx.h"
#include "WorldGeneratorFactory.h"

#include "generation/IWorldGenerator.h"
#include "generation/FlatWorldGenerator.h"

std::unique_ptr<IWorldGenerator> WorldGeneratorFactory::makeWorldGenerator(WorldGeneratorType type, World& world) {
	switch (type) {
		case WorldGeneratorType::Default:
			return std::make_unique<IWorldGenerator>(world);
        case WorldGeneratorType::Flat:
            return std::make_unique<FlatWorldGenerator>(world);
	}
    static_assert(e_cast(WorldGeneratorType::COUNT) == 2);

	return nullptr;
}
