#include "Hospital.hpp"
#include "../game.hpp"
#include "../map.hpp"
#include "../message.hpp"
#include "../generator.hpp"
#include "LocationUtils.hpp"
#include <algorithm>

void HospitalLocation::onEnter(Game& game, LabyrinthMap& /*map*/, const std::string& /*playerName*/, size_t /*x*/, size_t /*y*/, std::vector<std::string>& messages) {
	appendWire(messages, Message::HospitalEnter);
}

void HospitalLocation::onExit(Game& /*game*/, LabyrinthMap& /*map*/, const std::string& /*playerName*/, size_t /*x*/, size_t /*y*/, std::vector<std::string>& messages) {
	appendWire(messages, Message::HospitalExit);
}

void HospitalLocation::onPlaced(Game& /*game*/, LabyrinthMap& map) {
	using P = std::vector<std::pair<int,int>>;
	std::vector<P> patterns;
	size_t area = map.width * map.height;
	if (area <= 100) {
		patterns = {
			{{0,0},{1,0}},
			{{0,0},{0,1}},
			{{0,0},{1,0},{2,0}},
			{{0,0},{0,1},{0,2}},
			{{0,0},{1,0},{0,1}},
		};
	} else {
		patterns = {
			{{0,0},{1,0},{0,1},{1,1}},
			{{0,0},{1,0},{2,0},{0,1},{1,1},{2,1}},
			{{0,0},{1,0},{0,1},{1,1},{0,2},{1,2}},
			{{1,0},{0,1},{1,1},{2,1},{1,2}},
		};
	}
	std::mt19937 gen{rand_u32()};
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

