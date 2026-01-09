#pragma once
#include <vector>
#include <utility>
#include <random>

struct LabyrinthMap;
enum class CellContent;

namespace LocationUtils {

// Count connected components in the current passability graph (using walls)
size_t count_components(const LabyrinthMap& m);

// Try to place a cluster of cells of given type using provided patterns.
// Chooses anchor and opens exactly one entrance so that the maze remains a single component.
// Returns true on success and fills out_cells with absolute coordinates.
bool pick_and_place_location_cluster(
	LabyrinthMap& map,
	CellContent type,
	const std::vector<std::vector<std::pair<int,int>>>& patterns,
	std::mt19937& gen,
	std::vector<std::pair<size_t,size_t>>& out_cells
);

} // namespace LocationUtils


