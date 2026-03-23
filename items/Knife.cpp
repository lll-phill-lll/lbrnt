#include "../game.hpp"
#include "../map.hpp"
#include "Knife.hpp"

void Knife::apply(Game& game, LabyrinthMap& map, const std::string& playerName, Direction dir, Outcome& out) {
	auto ita = game.players.find(playerName);
	if (ita == game.players.end()) { out.logMessage(Message::InvalidTargetPlayer); return; }

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
	if (!can) {
		out.logMessage(Message::KnifeMiss, {dir_wire(dir)});
		out.logMessage(Message::KnifeSpent);
		return;
	}
	if (!victim.empty()) {
		if (attempt_kill(game, map, victim, out))
			out.logMessage(Message::KnifeHitPlayer, {dir_wire(dir), victim});
		out.logMessage(Message::KnifeSpent);
		return;
	}
	if (hit_bot_at(game, map, tx, ty, out)) {
		out.logMessage(Message::KnifeHitBot, {dir_wire(dir)});
		out.logMessage(Message::KnifeSpent);
		return;
	}
	out.logMessage(Message::KnifeMiss, {dir_wire(dir)});
	out.logMessage(Message::KnifeSpent);
}

