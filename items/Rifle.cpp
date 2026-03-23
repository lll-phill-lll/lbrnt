#include "../game.hpp"
#include "../map.hpp"
#include "Rifle.hpp"

static bool step_forward(const LabyrinthMap& map, size_t& x, size_t& y, Direction dir) {
	switch (dir) {
		case Direction::Left:  if (!map.can_move_left(x, y)) return false;  --x; return true;
		case Direction::Right: if (!map.can_move_right(x, y)) return false; ++x; return true;
		case Direction::Up:    if (!map.can_move_up(x, y)) return false;    --y; return true;
		case Direction::Down:  if (!map.can_move_down(x, y)) return false;  ++y; return true;
	}
	return false;
}

void Rifle::apply(Game& game, LabyrinthMap& map, const std::string& playerName, Direction dir, Outcome& out) {
	auto ita = game.players.find(playerName);
	if (ita == game.players.end()) { out.logMessage(Message::InvalidTargetPlayer); return; }

	size_t cx = ita->second.first, cy = ita->second.second;
	bool any = false;
	for (int i = 0; i < 3; ++i) {
		if (!step_forward(map, cx, cy, dir)) break;
		bool step_hit = false;
		for (const auto& kv : game.players) {
			if (kv.first == playerName) continue;
			if (kv.second.first == cx && kv.second.second == cy) {
				if (attempt_kill(game, map, kv.first, out))
					out.logMessage(Message::RifleHitPlayer, {dir_wire(dir), kv.first});
				any = true;
				step_hit = true;
			}
		}
		if (!step_hit && hit_bot_at(game, map, cx, cy, out)) {
			out.logMessage(Message::RifleHitBot, {dir_wire(dir)});
			any = true;
		}
	}
	if (!any) out.logMessage(Message::RifleMiss, {dir_wire(dir)});
}

