#include "stdafx.h"
#include "CityResidentManager.h"
#include "City.h"

CityResidentManager::CityResidentManager(City& city) :
    mCity(city)
{

}

void CityResidentManager::addResident(entt::entity entity) {
    // All residents start unemployed?
    mUnemployedResidents.push_back(entity);
}

void CityResidentManager::removeResident(entt::entity entity) {
    // TODO: check employment?
    for (size_t i = 0; i < mUnemployedResidents.size(); ++i) {
        if (mUnemployedResidents[i] == entity) {
            mUnemployedResidents[i] = mUnemployedResidents.back();
            mUnemployedResidents.pop_back();
            return;
        }
    }
    for (size_t i = 0; i < mEmployedResidents.size(); ++i) {
        if (mEmployedResidents[i] == entity) {
            mEmployedResidents[i] = mEmployedResidents.back();
            mEmployedResidents.pop_back();
            return;
        }
    }
}
