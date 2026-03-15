#pragma once
#include "Item.hpp"

struct Flashlight : public Item {
	const char* id() const override { return "flashlight"; }
	const char* displayName() const override { return "Фонарь"; }
	const char* description() const override { return "Освещает 3 клетки в выбранном направлении, показывая содержимое. Не тратит ход."; }
	const char* rechargeHint() const override { return "Одноразовый. Найдите новый на карте."; }
	void apply(Game& game, LabyrinthMap& map, const std::string& playerName, Direction dir, std::vector<std::string>& messages) override;
	int chargesPerUse() const override { return 1; }
	int defaultInitialCharges() const override { return 0; }
	bool persistsWhenDepleted() const override { return false; }
	void onDepleted(Game& game, LabyrinthMap& map, const std::string& playerName, std::vector<std::string>& messages) override;
};


