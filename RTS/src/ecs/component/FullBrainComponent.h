#pragma once

class City;
class Building;
class World;

#include "ai/tasks/IAgentTask.h"


// Represents a city and house, OPTIONAL
struct ResidentComponent {
    City* mCity;
    Building* mHome;
};

//struct ProfessionComponent {
    //ProfessionType profession; // Data drive?
//};

struct NeedsComponent {
    float hunger = 0.0f; // 0.0 = no hunger, 1.0 = dying of hunger
    float tiredness = 0.0f; // 0.0 = well rested, 1.0 = sleep deprived
};

struct FullBrainComponent {
    float x;
};
