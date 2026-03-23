#include "Exit.hpp"
#include "../game.hpp"
#include "../map.hpp"

void ExitLocation::onEnter(Game& game, LabyrinthMap& map, const std::string& playerName, size_t x, size_t y, Outcome& out) {
	(void)map; (void)x; (void)y;
	if (player_has_treasure(game, playerName)) {
		out.logMessage(Message::ExitLocationWithTreasure);
		game.finished = true;
	} else {
		out.logMessage(Message::ExitLocationWithoutTreasure);
	}
}

void ExitLocation::onExit(Game& /*game*/, LabyrinthMap& /*map*/, const std::string& /*playerName*/, size_t /*x*/, size_t /*y*/, Outcome& /*out*/) {
}

void ExitLocation::onPlaced(Game& /*game*/, LabyrinthMap& /*map*/) {
}
