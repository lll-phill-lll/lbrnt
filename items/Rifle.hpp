#pragma once
#include "Item.hpp"

struct Rifle : public Item {
	const char* id() const override { return "rifle"; }
	void apply(Game& game, LabyrinthMap& map, const std::string& playerName, Direction dir, std::vector<std::string>& messages) override;
	int chargesPerUse() const override { return 1; }
	int defaultInitialCharges() const override { return 0; }
	bool persistsWhenDepleted() const override { return true; }
};


