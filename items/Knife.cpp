#include "../game.hpp"
#include "../map.hpp"
#include "Knife.hpp"

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
	if (attempt_kill(game, map, victim, messages))
		messages.push_back(std::string("Удар ножом ") + dir_ru(dir) + ": игрок " + victim + " отправлен в больницу");
	messages.push_back("Нож потрачен");
}


