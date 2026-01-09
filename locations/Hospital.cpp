#include "Hospital.hpp"
#include "../game.hpp"
#include "../map.hpp"
#include "../generator.hpp"
#include "LocationUtils.hpp"
#include <algorithm>

void HospitalLocation::onEnter(Game& game, LabyrinthMap& /*map*/, const std::string& /*playerName*/, size_t /*x*/, size_t /*y*/, std::vector<std::string>& messages) {
	messages.push_back("Вы нашли больницу.");
}

void HospitalLocation::onExit(Game& /*game*/, LabyrinthMap& /*map*/, const std::string& /*playerName*/, size_t /*x*/, size_t /*y*/, std::vector<std::string>& messages) {
	messages.push_back("Вы покинули больницу.");
}

void HospitalLocation::onPlaced(Game& /*game*/, LabyrinthMap& map) {
	// Choose pattern
	using P = std::vector<std::pair<int,int>>;
	std::vector<P> patterns = {
		{{0,0},{1,0},{0,1},{1,1}},
		{{0,0},{1,0},{2,0},{0,1},{1,1},{2,1}},
		{{0,0},{1,0},{0,1},{1,1},{0,2},{1,2}},
		{{0,0},{1,0},{2,0},{0,1},{1,1},{2,1},{0,2},{1,2},{2,2}},
		{{1,0},{0,1},{1,1},{2,1},{1,2}},
	};
	std::mt19937 gen{rand_u32()};
	std::shuffle(patterns.begin(), patterns.end(), gen);
	// Candidates
	std::vector<std::tuple<size_t,size_t,P>> candidates;
	for (const auto& pat : patterns) {
		int max_dx = 0, max_dy = 0;
		for (auto [dx,dy] : pat) { if (dx > max_dx) max_dx = dx; if (dy > max_dy) max_dy = dy; }
		for (size_t sy = 0; sy + static_cast<size_t>(max_dy) < map.height; ++sy) {
			for (size_t sx = 0; sx + static_cast<size_t>(max_dx) < map.width; ++sx) {
				candidates.emplace_back(sx, sy, pat);
			}
		}
	}
	if (candidates.empty()) return;
	std::shuffle(candidates.begin(), candidates.end(), gen);
	for (auto& cand : candidates) (void)cand;
	// Use shared placement helper
	std::vector<std::pair<size_t,size_t>> dummy;
	LocationUtils::pick_and_place_location_cluster(map, CellContent::Hospital, patterns, gen, dummy);
}

bool HospitalLocation::teleportToHospital(Game& game, LabyrinthMap& map, const std::string& victim) {
	for (size_t y = 0; y < map.height; ++y) {
		for (size_t x = 0; x < map.width; ++x) {
			if (map.get_cell(x, y) == CellContent::Hospital) {
				game.players[victim] = {x, y};
				return true;
			}
		}
	}
	return false;
}

