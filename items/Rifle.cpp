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

void Rifle::apply(Game& game, LabyrinthMap& map, const std::string& playerName, Direction dir, std::vector<std::string>& messages) {
	auto ita = game.players.find(playerName);
	if (ita == game.players.end()) { messages.push_back("Нет такого игрока"); return; }

	size_t cx = ita->second.first, cy = ita->second.second;
	bool any = false;
	for (int i = 0; i < 3; ++i) {
		if (!step_forward(map, cx, cy, dir)) break;
		for (const auto& kv : game.players) {
			if (kv.first == playerName) continue;
			if (kv.second.first == cx && kv.second.second == cy) {
				if (attempt_kill(game, map, kv.first, messages))
					messages.push_back(std::string("Ружьё ") + dir_ru(dir) + ": игрок " + kv.first + " отправлен в больницу");
				any = true;
			}
		}
	}
	if (!any) messages.push_back(std::string("Ружьё ") + dir_ru(dir) + ": промах");
}


