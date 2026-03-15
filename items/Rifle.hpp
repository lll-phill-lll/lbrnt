#pragma once
#include "Item.hpp"

struct Rifle : public Item {
	const char* id() const override { return "rifle"; }
	const char* displayName() const override { return "Ружьё"; }
	const char* description() const override { return "Стреляет прямо на 3 клетки. Пуля останавливается перед стеной. Убитый телепортируется в больницу."; }
	const char* rechargeHint() const override { return "Восстанавливается при посещении арсенала."; }
	void apply(Game& game, LabyrinthMap& map, const std::string& playerName, Direction dir, std::vector<std::string>& messages) override;
	int chargesPerUse() const override { return 1; }
	int defaultInitialCharges() const override { return 0; }
	bool persistsWhenDepleted() const override { return true; }
};


