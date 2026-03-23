#pragma once
#include "Item.hpp"

struct Armor : public Item {
	const char* id() const override { return "armor"; }
	const char* displayName() const override { return "Броня"; }
	const char* description() const override { return "Пассивная защита. Поглощает один смертельный удар от ножа, дробовика или ружья. После этого уничтожается."; }
	const char* rechargeHint() const override { return "Одноразовая. Найдите новую на карте."; }
	void apply(Game& game, LabyrinthMap& map, const std::string& playerName, Direction dir, Outcome& out) override;
	int chargesPerUse() const override { return 0; }
	int defaultInitialCharges() const override { return 0; }
	bool persistsWhenDepleted() const override { return false; }
};
