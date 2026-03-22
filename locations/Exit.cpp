#include "Exit.hpp"
#include "../game.hpp"
#include "../map.hpp"

void ExitLocation::onEnter(Game& game, LabyrinthMap& map, const std::string& playerName, size_t x, size_t y, std::vector<std::string>& messages) {
	(void)map; (void)x; (void)y;
	if (player_has_treasure(game, playerName)) {
		messages.push_back("Выход найден! Игрок вынес сокровище!");
		game.finished = true;
	} else {
		messages.push_back("Выход найден, но без сокровища.");
	}
}

void ExitLocation::onExit(Game& /*game*/, LabyrinthMap& /*map*/, const std::string& /*playerName*/, size_t /*x*/, size_t /*y*/, std::vector<std::string>& /*messages*/) {
	// no-op
}

void ExitLocation::onPlaced(Game& /*game*/, LabyrinthMap& /*map*/) {
	// generator handles exit placement/opening
}


