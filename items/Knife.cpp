#include "../game.hpp"
#include "../map.hpp"
#include "../message.hpp"
#include "Knife.hpp"

void Knife::apply(Game& game, LabyrinthMap& map, const std::string& playerName, Direction dir, std::vector<std::string>& messages) {
	auto ita = game.players.find(playerName);
	if (ita == game.players.end()) { appendWire(messages, Message::InvalidTargetPlayer); return; }

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
		appendWire(messages, Message::KnifeMiss, {dir_ru(dir)});
		appendWire(messages, Message::KnifeSpent);
		return;
	}
	if (!victim.empty()) {
		if (attempt_kill(game, map, victim, messages))
			appendWire(messages, Message::KnifeHitPlayer, {dir_ru(dir), victim});
		appendWire(messages, Message::KnifeSpent);
		return;
	}
	if (hit_bot_at(game, map, tx, ty, messages)) {
		appendWire(messages, Message::KnifeHitBot, {dir_ru(dir)});
		appendWire(messages, Message::KnifeSpent);
		return;
	}
	appendWire(messages, Message::KnifeMiss, {dir_ru(dir)});
	appendWire(messages, Message::KnifeSpent);
}


