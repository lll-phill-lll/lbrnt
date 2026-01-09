#include "../game.hpp"
#include "../map.hpp"
#include "Shotgun.hpp"

static long long key_xy_local_sg(size_t x, size_t y) {
	return (long long)y * 1000000LL + (long long)x;
}

static bool step_forward_sg(const LabyrinthMap& map, size_t& x, size_t& y, Direction dir) {
	switch (dir) {
		case Direction::Left:  if (!map.can_move_left(x, y)) return false;  --x; return true;
		case Direction::Right: if (!map.can_move_right(x, y)) return false; ++x; return true;
		case Direction::Up:    if (!map.can_move_up(x, y)) return false;    --y; return true;
		case Direction::Down:  if (!map.can_move_down(x, y)) return false;  ++y; return true;
	}
	return false;
}

static void hospitalize_sg(Game& game, LabyrinthMap& map, const std::string& victim, std::vector<std::string>& messages) {
	bool sent = false;
	if (!map.hospital_cells.empty()) {
		auto [hx, hy] = map.hospital_cells.front();
		game.players[victim] = {hx, hy};
		sent = true;
	}
	if (sent) messages.push_back("Игрок " + victim + " отправлен в больницу");
	else messages.push_back("Больница не найдена");
}

void Shotgun::apply(Game& game, LabyrinthMap& map, const std::string& playerName, Direction dir, std::vector<std::string>& messages) {
	auto ita = game.players.find(playerName);
	if (ita == game.players.end()) { messages.push_back("Нет такого игрока"); return; }

	// Shotgun fires a 3-wide belt one cell ahead; blocked if first step is blocked
	size_t fx = ita->second.first, fy = ita->second.second;
	if (!step_forward_sg(map, fx, fy, dir)) { messages.push_back("Выстрел: стена перед вами"); return; }

	// Determine three target cells perpendicular to direction
	std::vector<std::pair<size_t,size_t>> targets;
	switch (dir) {
		case Direction::Up:
		case Direction::Down: {
			// width along X: (fx-1,fy), (fx,fy), (fx+1,fy)
			if (fx > 0) targets.emplace_back(fx - 1, fy);
			targets.emplace_back(fx, fy);
			if (fx + 1 < map.width) targets.emplace_back(fx + 1, fy);
			break;
		}
		case Direction::Left:
		case Direction::Right: {
			// width along Y: (fx,fy-1), (fx,fy), (fx,fy+1)
			if (fy > 0) targets.emplace_back(fx, fy - 1);
			targets.emplace_back(fx, fy);
			if (fy + 1 < map.height) targets.emplace_back(fx, fy + 1);
			break;
		}
	}

	bool any = false;
	for (auto [tx, ty] : targets) {
		for (const auto& kv : game.players) {
			if (kv.first == playerName) continue;
			if (kv.second.first == tx && kv.second.second == ty) {
				// drop treasure
				if (game.players_with_treasure.count(kv.first)) {
					game.players_with_treasure.erase(kv.first);
					game.loot_treasure[key_xy_local_sg(tx, ty)] += 1;
				}
				hospitalize_sg(game, map, kv.first, messages);
				any = true;
			}
		}
	}
	if (!any) messages.push_back("Выстрел: промах");
}


