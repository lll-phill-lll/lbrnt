#include "../game.hpp"
#include "../map.hpp"
#include "../locations/Location.hpp"
#include "../locations/Hospital.hpp"
#include "Knife.hpp"

static long long key_xy_local(size_t x, size_t y) {
	return (long long)y * 1000000LL + (long long)x;
}

void Knife::apply(Game& game, LabyrinthMap& map, const std::string& playerName, Direction dir, std::vector<std::string>& messages) {
	auto ita = game.players.find(playerName);
	if (ita == game.players.end()) { messages.push_back("Нет такого игрока"); return; }

	auto pos = ita->second;
	size_t tx = pos.first, ty = pos.second;
	bool can = false;
	switch (dir) {
		case Direction::Left:  can = map.can_move_left(tx, ty);  if (can) --tx; break;
		case Direction::Right: can = map.can_move_right(tx, ty); if (can) ++tx; break;
		case Direction::Up:    can = map.can_move_up(tx, ty);    if (can) --ty; break;
		case Direction::Down:  can = map.can_move_down(tx, ty);  if (can) ++ty; break;
	}
	// find victim at target
	std::string victim;
	for (const auto& kv : game.players) {
		if (kv.first == playerName) continue;
		if (kv.second.first == tx && kv.second.second == ty) { victim = kv.first; break; }
	}
	if (victim.empty() || !can) {
		messages.push_back(std::string("Удар ножом ") + dir_ru(dir) + " — мимо");
		messages.push_back("Нож потрачен");
		return;
	}
	// drop victim's treasure on ground
	if (game.players_with_treasure.count(victim)) {
		game.players_with_treasure.erase(victim);
		game.loot_treasure[key_xy_local(tx, ty)] += 1;
	}
	// teleport victim to hospital via location
	bool sent = false;
	if (auto* loc = getLocationFor(CellContent::Hospital)) {
		// HospitalLocation provides teleport method
		if (auto* hosp = dynamic_cast<HospitalLocation*>(loc)) {
			sent = hosp->teleportToHospital(game, map, victim);
		}
	}
	if (sent) messages.push_back(std::string("Удар ножом ") + dir_ru(dir) + ": игрок " + victim + " отправлен в больницу");
	else messages.push_back("Больница не найдена");
	messages.push_back("Нож потрачен");
}


