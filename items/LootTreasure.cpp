#include "LootTreasure.hpp"
#include "../game.hpp"
#include "../map.hpp"
#include "../message.hpp"

void LootTreasure::apply(Game& /*game*/, LabyrinthMap& /*map*/, const std::string& /*playerName*/, Direction /*dir*/, std::vector<std::string>& messages) {
	appendWire(messages, Message::TreasureUseHint);
}
