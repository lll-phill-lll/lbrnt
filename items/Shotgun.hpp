#pragma once
#include "Item.hpp"

struct Shotgun : public Item {
	const char* id() const override { return "shotgun"; }
	const char* displayName() const override { return "Дробовик"; }
	const char* description() const override { return "Стреляет на 1 клетку вперёд, поражая 3 клетки в ширину. Убитый телепортируется в больницу."; }
	const char* rechargeHint() const override { return "Восстанавливается при посещении арсенала."; }
	void apply(Game& game, LabyrinthMap& map, const std::string& playerName, Direction dir, std::vector<std::string>& messages) override;
	int chargesPerUse() const override { return 1; }
	int defaultInitialCharges() const override { return 0; }
	bool persistsWhenDepleted() const override { return true; }
};


