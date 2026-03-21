#include "Armor.hpp"

void Armor::apply(Game& /*game*/, LabyrinthMap& /*map*/, const std::string& /*playerName*/, Direction /*dir*/, std::vector<std::string>& messages) {
	messages.push_back("Броня — пассивный предмет. Её нельзя использовать.");
}
