#pragma once
#include <string>
#include <unordered_map>

struct Inventory {
	// Minimal extensible inventory: states per item id
	// For now we track only "knife_broken" but structure allows more
	std::unordered_map<std::string, int> ints;   // generic integer states (e.g., ammo)
	std::unordered_map<std::string, bool> flags; // generic flags (e.g., knife_broken)

	bool isKnifeBroken() const {
		auto it = flags.find("knife_broken");
		return it != flags.end() && it->second;
	}
	void setKnifeBroken(bool v) {
		flags["knife_broken"] = v;
	}
};


