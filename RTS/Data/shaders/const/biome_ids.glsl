//This file is generated at runtime by code, do not edit it (TODO: Turn this into a UBO For better perf)
const int BIOME_Ocean = 0;
const int BIOME_Plains = 1;
const int BIOME_Plains_B = 2;
const int BIOME_Plains_C = 3;
const int BIOME_Mountains = 4;
const int BIOME_Mountains_B = 5;
const int BIOME_Mountains_C = 6;
const int BIOME_Forest = 7;
const int BIOME_Forest_B = 8;
const int BIOME_Forest_C = 9;
const int BIOME_Hotsprings = 10;
const int BIOME_Hotsprings_B = 11;
const int BIOME_Hotsprings_C = 12;

const uint BIOME_CORRUPTION[13] = {
    0, // Ocean
    0, // Plains
    1, // Plains_B
    2, // Plains_C
    0, // Mountains
    1, // Mountains_B
    2, // Mountains_C
    0, // Forest
    1, // Forest_B
    2, // Forest_C
    0, // Hotsprings
    1, // Hotsprings_B
    2, // Hotsprings_C
};

const uvec3 BIOME_TRANSFORM[13] = {
    uvec3(0,0,0), // Ocean
    uvec3(1,2,3), // Plains
    uvec3(2,2,2), // Plains_B
    uvec3(3,3,3), // Plains_C
    uvec3(4,5,6), // Mountains
    uvec3(5,5,5), // Mountains_B
    uvec3(6,6,6), // Mountains_C
    uvec3(7,8,9), // Forest
    uvec3(8,8,8), // Forest_B
    uvec3(9,9,9), // Forest_C
    uvec3(10,11,12), // Hotsprings
    uvec3(11,11,11), // Hotsprings_B
    uvec3(12,12,12), // Hotsprings_C
};

const vec3 BIOME_COLORS[13] = {
    vec3(0.000000, 0.000000, 1.000000), // Ocean
    vec3(0.501961, 0.501961, 0.000000), // Plains
    vec3(1.000000, 0.250980, 0.000000), // Plains_B
    vec3(0.501961, 0.501961, 1.000000), // Plains_C
    vec3(0.501961, 0.501961, 0.501961), // Mountains
    vec3(1.000000, 0.501961, 0.501961), // Mountains_B
    vec3(0.501961, 0.501961, 1.000000), // Mountains_C
    vec3(0.000000, 0.501961, 0.000000), // Forest
    vec3(1.000000, 0.501961, 0.000000), // Forest_B
    vec3(0.000000, 0.392157, 0.784314), // Forest_C
    vec3(0.000000, 0.501961, 0.501961), // Hotsprings
    vec3(1.000000, 0.501961, 0.501961), // Hotsprings_B
    vec3(0.000000, 0.501961, 1.000000), // Hotsprings_C
};
