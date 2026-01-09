#pragma once
#include <string>
#include <vector>

struct Game;
struct LabyrinthMap;
enum class Direction;

struct Item {
	virtual ~Item() = default;
	virtual const char* id() const = 0;
	virtual void apply(Game& game, LabyrinthMap& map, const std::string& playerName, Direction dir, std::vector<std::string>& messages) = 0;
	// item consumption profile
	virtual int chargesPerUse() const { return 1; }
	virtual int defaultInitialCharges() const { return 0; }
};


