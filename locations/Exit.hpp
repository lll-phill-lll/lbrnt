#pragma once
#include "Location.hpp"

struct ExitLocation : public Location {
	const char* id() const override { return "exit"; }
	void onEnter(Game& game, LabyrinthMap& map, const std::string& playerName, size_t x, size_t y, std::vector<std::string>& messages) override;
	void onExit(Game& game, LabyrinthMap& map, const std::string& playerName, size_t x, size_t y, std::vector<std::string>& messages) override;
	void onPlaced(Game& game, LabyrinthMap& map) override;
};


