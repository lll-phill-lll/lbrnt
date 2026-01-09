#pragma once
#include "Location.hpp"

struct HospitalLocation : public Location {
	const char* id() const override { return "hospital"; }
	void onEnter(Game& game, LabyrinthMap& map, const std::string& playerName, size_t x, size_t y, std::vector<std::string>& messages) override;
	void onPlaced(Game& game, LabyrinthMap& map) override;
	void onExit(Game& game, LabyrinthMap& map, const std::string& playerName, size_t x, size_t y, std::vector<std::string>& messages) override;
	// Teleport victim to a hospital cell (returns true if teleported)
	bool teleportToHospital(Game& game, LabyrinthMap& map, const std::string& victim);
private:
	std::vector<std::pair<size_t,size_t>> cells_;
};


