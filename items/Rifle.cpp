#include "../game.hpp"
#include "../map.hpp"
#include "Rifle.hpp"

static long long key_xy_local_r(size_t x, size_t y) {
	return (long long)y * 1000000LL + (long long)x;
}

static bool step_forward(const LabyrinthMap& map, size_t& x, size_t& y, Direction dir) {
	switch (dir) {
		case Direction::Left:  if (!map.can_move_left(x, y)) return false;  --x; return true;
		case Direction::Right: if (!map.can_move_right(x, y)) return false; ++x; return true;
		case Direction::Up:    if (!map.can_move_up(x, y)) return false;    --y; return true;
		case Direction::Down:  if (!map.can_move_down(x, y)) return false;  ++y; return true;
	}
	return false;
}

static void hospitalize(Game& game, LabyrinthMap& map, const std::string& victim, std::vector<std::string>& messages) {
	bool sent = false;
	for (size_t yy = 0; yy < map.height && !sent; ++yy) {
		for (size_t xx = 0; xx < map.width && !sent; ++xx) {
			if (map.get_cell(xx, yy) == CellContent::Hospital) {
				game.players[victim] = {xx, yy};
				sent = true;
			}
		}
	}
	if (sent) messages.push_back("Игрок " + victim + " отправлен в больницу");
	else messages.push_back("Больница не найдена");
}

void Rifle::apply(Game& game, LabyrinthMap& map, const std::string& playerName, Direction dir, std::vector<std::string>& messages) {
	auto ita = game.players.find(playerName);
	if (ita == game.players.end()) { messages.push_back("Нет такого игрока"); return; }

	size_t cx = ita->second.first, cy = ita->second.second;
	bool any = false;
	// Shoot straight up to 3 cells, blocked by walls between steps
	for (int i = 0; i < 3; ++i) {
		if (!step_forward(map, cx, cy, dir)) break;
		// hit any players on this cell
		for (const auto& kv : game.players) {
			if (kv.first == playerName) continue;
			if (kv.second.first == cx && kv.second.second == cy) {
				// drop treasure
				if (game.players_with_treasure.count(kv.first)) {
					game.players_with_treasure.erase(kv.first);
					game.loot_treasure[key_xy_local_r(cx, cy)] += 1;
				}
				hospitalize(game, map, kv.first, messages);
				any = true;
			}
		}
	}
	if (!any) messages.push_back("Выстрел: промах");
}


