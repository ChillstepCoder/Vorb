#pragma once

class City;
class Building;
class World;

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

enum class PersonAITask {
    IDLE,
    CHOP_WOOD,
    BUILD,
    SLEEP,
};

// Humans, elves
struct PersonAIComponent {
    // Memories
    // Relationships
    // Tasks
    // States
    // Perception
    // Needs
    PersonAITask currentTask = PersonAITask::IDLE;
    City* mCity = nullptr;
    // TODO: Polymorphism?
    void* mCurrentTaskData = nullptr;

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

    void update(entt::registry& registry, float deltaTime);

    World& mWorld;
};
