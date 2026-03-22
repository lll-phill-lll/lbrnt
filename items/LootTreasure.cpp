#include "LootTreasure.hpp"
#include "../game.hpp"
#include "../map.hpp"

void LootTreasure::apply(Game& /*game*/, LabyrinthMap& /*map*/, const std::string& /*playerName*/, Direction /*dir*/, std::vector<std::string>& messages) {
	messages.push_back("Сокровище нельзя «использовать» — дойдите до выхода с внешней стороны лабиринта.");
}
