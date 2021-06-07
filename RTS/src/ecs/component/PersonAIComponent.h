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

struct FactionComponent {

};

struct NeedsComponent {
    float hunger = 0.0f; // 0.0 = no hunger, 1.0 = dying of hunger
    float tiredness = 0.0f; // 0.0 = well rested, 1.0 = sleep deprived
};

// Humans, elves
// TODO: mCurrentTask is a raw pointer and can leak
struct PersonAIComponent {
    // Memories
    // Relationships
    // Tasks
    // States
    // Perception
    // Needs
    City* mCity = nullptr;
    IAgentTaskPtr mCurrentTask;

    //bool inCombat = false;
    //float inCombatTime = 0.0f;
};

// Everything that encompasses a "Person" can be defined with a series of components
// ResidentComponent - Represents a city and house, OPTIONAL
// ProfessionComponent - What job I have.

// AISystems (Update AI components)
// Pathfinder
// Tasks
// 

class PersonAISystem {
public:
    PersonAISystem(World& world);

    void update(entt::registry& registry);

    World& mWorld;
};
