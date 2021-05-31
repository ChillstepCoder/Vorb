#include "stdafx.h"
#include "CityBusinessManager.h"

#include "ecs/business/BusinessComponent.h"

#include "city/City.h"
#include "World.h"
#include "ecs/EntityComponentSystem.h"
#include "ecs/component/EmployeeComponent.h"

CityBusinessManager::CityBusinessManager(City& city) : mCity(city)
{

}

void CityBusinessManager::registerBusiness(entt::entity businessEntity)
{
    mBusinessEntities.emplace_back(businessEntity);
}

bool CityBusinessManager::tryEmploy(entt::entity personToEmploy)
{
    entt::registry& registry = mCity.mWorld.getECS().mRegistry;

    entt::entity bestBusiness = INVALID_ENTITY;
    ui32 bestScore = 0;

    auto&& view = registry.view<BusinessComponent>();
    
    for (auto entity : view) {
        auto& cmp = view.get<BusinessComponent>(entity);
        size_t employeeCount = cmp.mEmployees.size();
        if (employeeCount < cmp.mMaxEmployeeCount) {
            // Need large number since we can go negative on score influence if we have more than
            // desired
            ui32 score = 100000 + (cmp.mDesiredEmployeeCount - employeeCount);
            // Favor the first few employees heavily
            if (employeeCount < cmp.mDesiredEmployeeCount) {
                if (employeeCount == 0) {
                    score += 1000;
                }
                else if (employeeCount == 1) {
                    score += 100;
                }
                else if (employeeCount == 2) {
                    score += 10;
                }
            }
            if (score > bestScore) {
                bestScore = score;
                bestBusiness = entity;
            }
        }
    }

    if (bestBusiness != INVALID_ENTITY) {
        assert(!registry.try_get<EmployeeComponent>(personToEmploy));

        auto&& employeeComponent = registry.emplace<EmployeeComponent>(personToEmploy);
        employeeComponent.mBusiness = bestBusiness;

        auto&& businessCmp = registry.get<BusinessComponent>(bestBusiness);
        businessCmp.mEmployees.emplace_back(personToEmploy);
        return true;
    }

    return false;
}
