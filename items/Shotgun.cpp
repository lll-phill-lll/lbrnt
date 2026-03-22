#include "../game.hpp"
#include "../map.hpp"
#include "Shotgun.hpp"

static bool step_forward_sg(const LabyrinthMap& map, size_t& x, size_t& y, Direction dir) {
	switch (dir) {
		case Direction::Left:  if (!map.can_move_left(x, y)) return false;  --x; return true;
		case Direction::Right: if (!map.can_move_right(x, y)) return false; ++x; return true;
		case Direction::Up:    if (!map.can_move_up(x, y)) return false;    --y; return true;
		case Direction::Down:  if (!map.can_move_down(x, y)) return false;  ++y; return true;
	}
	return false;
}

void Shotgun::apply(Game& game, LabyrinthMap& map, const std::string& playerName, Direction dir, Outcome& out) {
	auto ita = game.players.find(playerName);
	if (ita == game.players.end()) { out.logMessage(Message::InvalidTargetPlayer); return; }

	size_t fx = ita->second.first, fy = ita->second.second;
	if (!step_forward_sg(map, fx, fy, dir)) { out.logMessage(Message::ShotgunWall, {dir_wire(dir)}); return; }

	std::vector<std::pair<size_t,size_t>> targets;
	switch (dir) {
		case Direction::Up:
		case Direction::Down: {
			if (fx > 0) targets.emplace_back(fx - 1, fy);
			targets.emplace_back(fx, fy);
			if (fx + 1 < map.width) targets.emplace_back(fx + 1, fy);
			break;
		}
		case Direction::Left:
		case Direction::Right: {
			if (fy > 0) targets.emplace_back(fx, fy - 1);
			targets.emplace_back(fx, fy);
			if (fy + 1 < map.height) targets.emplace_back(fx, fy + 1);
			break;
		}
	}

	bool any = false;
	for (auto [tx, ty] : targets) {
		bool cell_hit = false;
		for (const auto& kv : game.players) {
			if (kv.first == playerName) continue;
			if (kv.second.first == tx && kv.second.second == ty) {
				if (attempt_kill(game, map, kv.first, out))
					out.logMessage(Message::ShotgunHitPlayer, {dir_wire(dir), kv.first});
				any = true;
				cell_hit = true;
			}
		}
		if (!cell_hit && hit_bot_at(game, map, tx, ty, out)) {
			out.logMessage(Message::ShotgunHitBot, {dir_wire(dir)});
			any = true;
		}
	}
	if (!any) out.logMessage(Message::ShotgunMiss, {dir_wire(dir)});
}

