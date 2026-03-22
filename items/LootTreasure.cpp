#include "LootTreasure.hpp"
#include "../game.hpp"
#include "../map.hpp"

void LootTreasure::apply(Game& /*game*/, LabyrinthMap& /*map*/, const std::string& /*playerName*/, Direction /*dir*/, Outcome& out) {
	out.logMessage(Message::TreasureUseHint);
}
