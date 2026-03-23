#pragma once
#include "Item.hpp"

struct Knife : public Item {
	const char* id() const override { return "knife"; }
	const char* displayName() const override { return "Нож"; }
	const char* description() const override { return "Бьёт на 1 клетку в выбранном направлении. Убитый игрок телепортируется в больницу."; }
	const char* rechargeHint() const override { return "Восстанавливается при посещении арсенала."; }
	void apply(Game& game, LabyrinthMap& map, const std::string& playerName, Direction dir, Outcome& out) override;
	int chargesPerUse() const override { return 1; }
	int defaultInitialCharges() const override { return 1; }
	bool persistsWhenDepleted() const override { return true; }
};


