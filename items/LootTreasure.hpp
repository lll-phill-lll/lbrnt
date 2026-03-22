#pragma once
#include "Item.hpp"

/** Сокровище как предмет в инвентаре (charges = сколько единиц несёт игрок). */
struct LootTreasure : public Item {
	const char* id() const override { return "treasure"; }
	const char* displayName() const override { return "Сокровище"; }
	const char* description() const override { return "Несите к выходу с лабиринта."; }
	const char* rechargeHint() const override { return "Подбирается на клетке сокровища или с кучи на земле."; }
	void apply(Game& game, LabyrinthMap& map, const std::string& playerName, Direction dir, std::vector<std::string>& messages) override;
	/** Как у брони: «использование» не тратит заряд — несёте до выхода. */
	int chargesPerUse() const override { return 0; }
	bool persistsWhenDepleted() const override { return true; }
};
