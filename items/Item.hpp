#pragma once
#include <string>

struct Game;
struct LabyrinthMap;
struct Outcome;
enum class Direction;

struct Item {
	virtual ~Item() = default;
	virtual const char* id() const = 0;
	virtual const char* displayName() const = 0;
	virtual const char* description() const = 0;
	virtual const char* rechargeHint() const = 0;
	virtual void apply(Game& game, LabyrinthMap& map, const std::string& playerName, Direction dir, Outcome& out) = 0;
	// item consumption profile
	virtual int chargesPerUse() const { return 1; }
	virtual int defaultInitialCharges() const { return 0; }
	// behavior on depletion
	virtual bool persistsWhenDepleted() const { return false; }
	virtual void onDepleted(Game& /*game*/, LabyrinthMap& /*map*/, const std::string& /*playerName*/, Outcome& /*out*/) {}
};

/** Проверка зарядов, apply(), списание — общий путь для use_item. */
bool item_use(Game& game, Item& item, LabyrinthMap& map, const std::string& playerName, Direction dir, Outcome& out);


