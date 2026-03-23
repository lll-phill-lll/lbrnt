#include "Armor.hpp"
#include "../game.hpp"

void Armor::apply(Game& /*game*/, LabyrinthMap& /*map*/, const std::string& /*playerName*/, Direction /*dir*/, Outcome& out) {
	out.logMessage(Message::ArmorPassive);
}
