#include "Arsenal.hpp"
#include "../game.hpp"
#include "../map.hpp"
#include "../generator.hpp"
#include "LocationUtils.hpp"
#include <algorithm>

void ArsenalLocation::onEnter(Game& game, LabyrinthMap& /*map*/, const std::string& playerName, size_t /*x*/, size_t /*y*/, std::vector<std::string>& messages) {
	messages.push_back("Вы нашли арсенал.");
	// Repair knife to 1 charge if broken or zero
	int& k = game.item_charges[playerName]["knife"];
	if (k <= 0) {
		k = 1;
		game.broken_knife.erase(playerName);
		messages.push_back("Ваш нож починен.");
	}
}

// Log exit
void ArsenalLocation::onExit(Game& /*game*/, LabyrinthMap& /*map*/, const std::string& /*playerName*/, size_t /*x*/, size_t /*y*/, std::vector<std::string>& messages) {
	messages.push_back("Вы покинули арсенал.");
}

void ArsenalLocation::onPlaced(Game& /*game*/, LabyrinthMap& map) {
	using P = std::vector<std::pair<int,int>>;
	std::vector<P> patterns = {
		{{0,0},{1,0},{0,1},{1,1}},
		{{0,0},{1,0},{2,0},{0,1},{1,1},{2,1}},
		{{0,0},{1,0},{0,1},{1,1},{0,2},{1,2}},
		{{0,0},{1,0},{2,0},{0,1},{1,1},{2,1},{0,2},{1,2},{2,2}},
		{{1,0},{0,1},{1,1},{2,1},{1,2}},
	};
	std::mt19937 gen{rand_u32()};
	LocationUtils::pick_and_place_location_cluster(map, CellContent::Arsenal, patterns, gen, cells_);
}


