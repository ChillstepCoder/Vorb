#pragma once
class City;

class CityBusinessManager
{
public:
    CityBusinessManager(City& city);

    void registerBusiness(entt::entity businessEntity);
    bool tryEmploy(entt::entity personToEmploy);

private:
    City& mCity;
    std::vector<entt::entity> mBusinessEntities;
};

