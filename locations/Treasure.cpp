#include "Treasure.hpp"
#include "../game.hpp"
#include "../map.hpp"

void TreasureLocation::onEnter(Game& game, LabyrinthMap& map, const std::string& playerName, size_t x, size_t y, Outcome& out) {
	if (map.get_cell(x, y) == CellContent::Treasure) {
		out.logMessage(Message::TreasureSpotFound);
		auto& inventory = game.inventories[playerName];
		int currentCharges = inventory.getCharges("treasure");
		if (currentCharges <= 0) {
			inventory.setCharges("treasure", 1);
		} else {
			inventory.setCharges("treasure", currentCharges + 1);
		}
		map.set_cell(x, y, CellContent::Empty);
	}
}

void TreasureLocation::onExit(Game& /*game*/, LabyrinthMap& /*map*/, const std::string& /*playerName*/, size_t /*x*/, size_t /*y*/, Outcome& /*out*/) {
}

void TreasureLocation::onPlaced(Game& /*game*/, LabyrinthMap& /*map*/) {
}
