#pragma once
#include <string>

struct Game;
struct LabyrinthMap;
struct Outcome;
enum class CellContent;

struct Location {
	virtual ~Location() = default;
	virtual const char* id() const = 0;
	// Called when a player stands on a cell of this location
	virtual void onEnter(Game& game, LabyrinthMap& map, const std::string& playerName, size_t x, size_t y, Outcome& out) = 0;
	// Called after location cells are placed on the map (can adjust walls etc.)
	// Implementations should choose their own shape and placement based on global seed.
	virtual void onPlaced(Game& game, LabyrinthMap& map) = 0;
	// Called when a player leaves a cell of this location
	virtual void onExit(Game& game, LabyrinthMap& map, const std::string& playerName, size_t x, size_t y, Outcome& out) = 0;
};

// Registry helpers
Location* getLocationFor(CellContent c);



