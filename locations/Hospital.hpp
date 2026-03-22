#pragma once
#include "Location.hpp"

struct HospitalLocation : public Location {
	const char* id() const override { return "hospital"; }
	void onEnter(Game& game, LabyrinthMap& map, const std::string& playerName, size_t x, size_t y, Outcome& out) override;
	void onPlaced(Game& game, LabyrinthMap& map) override;
	void onExit(Game& game, LabyrinthMap& map, const std::string& playerName, size_t x, size_t y, Outcome& out) override;
	// Teleport victim to a hospital cell (returns true if teleported)
	bool teleportToHospital(Game& game, LabyrinthMap& map, const std::string& victim);
};


