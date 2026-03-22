#include "Treasure.hpp"
#include "../game.hpp"
#include "../map.hpp"

void TreasureLocation::onEnter(Game& game, LabyrinthMap& map, const std::string& playerName, size_t x, size_t y, std::vector<std::string>& messages) {
	if (map.get_cell(x, y) == CellContent::Treasure) {
		messages.push_back("Нашёл сокровище!");
		game.inventories[playerName].setCharges("treasure", 1);
		map.set_cell(x, y, CellContent::Empty);
	}
}

void TreasureLocation::onExit(Game& /*game*/, LabyrinthMap& /*map*/, const std::string& /*playerName*/, size_t /*x*/, size_t /*y*/, std::vector<std::string>& /*messages*/) {
	// no-op
}

void TreasureLocation::onPlaced(Game& /*game*/, LabyrinthMap& /*map*/) {
	// generator already places treasure
}


