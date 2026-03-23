#pragma once
#include "Location.hpp"

struct TreasureLocation : public Location {
	const char* id() const override { return "treasure"; }
	void onEnter(Game& game, LabyrinthMap& map, const std::string& playerName, size_t x, size_t y, Outcome& out) override;
	void onExit(Game& game, LabyrinthMap& map, const std::string& playerName, size_t x, size_t y, Outcome& out) override;
	void onPlaced(Game& game, LabyrinthMap& map) override;
};


