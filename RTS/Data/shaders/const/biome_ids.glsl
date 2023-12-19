const int BIOME_OCEAN = 0;
const int BIOME_PLAINS = 1;
const int BIOME_MOUNTAINS = 2;
const int BIOME_FOREST = 3;
const int BIOME_HOTSPRINGS = 4;

const bool BIOME_SPREADABLE[5] = {
    false, // OCEAN
    false, // PLAINS
    true, // MOUNTAINS
    false, // FOREST
    true   // HOTSPRINGS
};

const bool BIOME_OVERRIDABLE[5] = {
    false, // OCEAN
    true, // PLAINS
    true, // MOUNTAINS
    true, // FOREST
    true   // HOTSPRINGS
};

const vec3 BIOME_COLORS[5] = {
    vec3(0.0, 0.4, 1.0), // OCEAN
    vec3(1.0, 0.0, 0.0), // PLAINS
    vec3(1.0, 0.0, 1.0), // MOUNTAINS
    vec3(0.0, 1.0, 0.0), // FOREST
    vec3(0.0, 1.0, 1.0), // HOT SPRINGS
};