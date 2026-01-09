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
	// behavior on depletion
	virtual bool persistsWhenDepleted() const { return false; } // remain in inventory with 0 charges
	virtual void onDepleted(Game& /*game*/, LabyrinthMap& /*map*/, const std::string& /*playerName*/, std::vector<std::string>& /*messages*/) {}
};


