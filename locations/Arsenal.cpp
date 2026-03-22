#include "Arsenal.hpp"
#include "../game.hpp"
#include "../map.hpp"
#include "../message.hpp"
#include "../generator.hpp"
#include "LocationUtils.hpp"
#include <algorithm>

void ArsenalLocation::onEnter(Game& game, LabyrinthMap& /*map*/, const std::string& playerName, size_t /*x*/, size_t /*y*/, std::vector<std::string>& messages) {
	appendWire(messages, Message::ArsenalEnter);
	// Repair all items present in inventory to at least 1 charge
	auto it = game.inventories.find(playerName);
	if (it == game.inventories.end()) return;
	auto& inv = it->second;
	for (auto& kv : inv.item_charges) {
		const std::string& itemId = kv.first;
		int& charges = kv.second;
		if (charges <= 0) {
			charges = 1;
			if (itemId == "knife") {
				game.broken_knife.erase(playerName);
				messages.push_back("Ваш нож починен.");
			} else if (itemId == "rifle") {
				messages.push_back("Ружьё перезаряжено.");
			} else if (itemId == "shotgun") {
				messages.push_back("Дробовик перезаряжен.");
			} else if (itemId == "flashlight") {
				messages.push_back("Фонарь заряжен.");
			} else {
				messages.push_back("Предмет перезаряжен: " + itemId);
			}
		}
	}
}

// Log exit
void ArsenalLocation::onExit(Game& /*game*/, LabyrinthMap& /*map*/, const std::string& /*playerName*/, size_t /*x*/, size_t /*y*/, std::vector<std::string>& messages) {
	appendWire(messages, Message::ArsenalExit);
}

void ArsenalLocation::onPlaced(Game& /*game*/, LabyrinthMap& map) {
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
	LocationUtils::pick_and_place_location_cluster(map, CellContent::Arsenal, patterns, gen, dummy);
}


