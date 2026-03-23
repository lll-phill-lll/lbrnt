#pragma once
#include "Location.hpp"

struct ArsenalLocation : public Location {
	const char* id() const override { return "arsenal"; }
	void onEnter(Game& game, LabyrinthMap& map, const std::string& playerName, size_t x, size_t y, Outcome& out) override;
	void onPlaced(Game& game, LabyrinthMap& map) override;
	void onExit(Game& game, LabyrinthMap& map, const std::string& playerName, size_t x, size_t y, Outcome& out) override;
};


