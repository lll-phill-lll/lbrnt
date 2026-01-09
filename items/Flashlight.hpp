#pragma once
#include "Item.hpp"

struct Flashlight : public Item {
	const char* id() const override { return "flashlight"; }
	void apply(Game& game, LabyrinthMap& map, const std::string& playerName, Direction dir, std::vector<std::string>& messages) override;
	int chargesPerUse() const override { return 1; }
	int defaultInitialCharges() const override { return 0; }
};


