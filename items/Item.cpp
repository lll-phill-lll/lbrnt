#include "Item.hpp"
#include "../game.hpp"
#include "../map.hpp"

// Centralized item use wrapper: checks/consumes charges and runs apply()
bool item_use(Game& game, Item& item, LabyrinthMap& map, const std::string& playerName, Direction dir, Outcome& out) {
	// ensure inventory exists
	Inventory& inv = game.inventories[playerName];
	int charges = inv.getCharges(item.id());
	const int spend = item.chargesPerUse();
	if (charges < spend) {
		if (std::string(item.id()) == "knife") out.logMessage(Message::ItemBroken);
		else out.logMessage(Message::ItemDepleted);
		return false;
	}
	// run effect
	item.apply(game, map, playerName, dir, out);
	// consume
	charges -= spend;
	inv.setCharges(item.id(), charges);
	// sync knife flag
	if (std::string(item.id()) == "knife") {
		if (charges <= 0) game.broken_knife.insert(playerName);
		else game.broken_knife.erase(playerName);
	}
	// depletion behavior
	if (charges <= 0 && !item.persistsWhenDepleted()) {
		item.onDepleted(game, map, playerName, out);
		inv.removeItem(item.id());
	}
	return true;
}


