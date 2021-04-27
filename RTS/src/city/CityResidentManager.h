#pragma once

class City;

struct ResidentData {

};

class CityResidentManager
{
public:
    CityResidentManager(City& city);

    void addResident(entt::entity entity);
    void removeResident(entt::entity entity); // TODO: Notify?

    size_t getPopulation() const { return mUnemployedResidents.size() + mEmployedResidents.size(); }
    // Businesses?
private:
    std::vector<entt::entity> mUnemployedResidents;
    std::vector<entt::entity> mEmployedResidents;
    City& mCity;
};

