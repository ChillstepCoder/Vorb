#pragma once

class City;

// ?
struct ResidentData {

};

// Database of all residents in a city
class CityResidentManager
{
public:
    CityResidentManager(City& city);

    void addResident(entt::entity entity);
    void removeResident(entt::entity entity); // TODO: Notify?

    size_t getPopulation() const { return mResidents.size(); }
    // Businesses?
private:
    std::vector<entt::entity> mResidents;
    City& mCity;
};

