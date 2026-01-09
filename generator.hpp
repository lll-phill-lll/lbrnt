#pragma once
#include "map.hpp"

void carve_maze(LabyrinthMap& map);
void place_items(LabyrinthMap& map);
void remove_extra_walls(LabyrinthMap& map, float openness);
void place_exit_edge(LabyrinthMap& map);
void place_hospital_cluster(LabyrinthMap& map);
void place_arsenal_cluster(LabyrinthMap& map);
void ensure_exit_open(LabyrinthMap& map);
LabyrinthMap generate_maze_with_items(size_t width, size_t height, float openness = 0.0f);
// Set seed for internal generator used by maze generation/place_* utilities
void set_rng_seed(unsigned int seed);


