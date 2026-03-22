#include "Armor.hpp"
#include "../message.hpp"

void Armor::apply(Game& /*game*/, LabyrinthMap& /*map*/, const std::string& /*playerName*/, Direction /*dir*/, std::vector<std::string>& messages) {
	appendWire(messages, Message::ArmorPassive);
}
