#include "stdafx.h"
#include "CityResidentManager.h"
#include "City.h"

CityResidentManager::CityResidentManager(City& city) :
    mCity(city)
{

}

void CityResidentManager::addResident(entt::entity entity) {
    // All residents start unemployed?
    mResidents.push_back(entity);
}

void CityResidentManager::removeResident(entt::entity entity) {
    // TODO: Notify employment? Resident himself should notify employer
    for (size_t i = 0; i < mResidents.size();) {
        if (mResidents[i] == entity) {
            mResidents[i] = mResidents.back();
            mResidents.pop_back();
            return;
        }
        else {
            ++i;
        }
    }
}
