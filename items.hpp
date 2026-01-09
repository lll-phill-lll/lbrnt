#pragma once
#include <string>
#include <unordered_map>

struct Inventory {
	// Minimal extensible inventory: states per item id
	// For now we track only "knife_broken" but structure allows more
	std::unordered_map<std::string, int> ints;   // generic integer states (e.g., ammo)
	std::unordered_map<std::string, bool> flags; // generic flags (e.g., knife_broken)
	// item charges by itemId
	std::unordered_map<std::string, int> item_charges;

	bool isKnifeBroken() const {
		auto it = flags.find("knife_broken");
		return it != flags.end() && it->second;
	}
	void setKnifeBroken(bool v) {
		flags["knife_broken"] = v;
	}
	int getCharges(const std::string& itemId) const {
		auto it = item_charges.find(itemId);
		return it == item_charges.end() ? 0 : it->second;
	}
	void setCharges(const std::string& itemId, int v) {
		item_charges[itemId] = v;
	}
	void removeItem(const std::string& itemId) {
		item_charges.erase(itemId);
	}
};


