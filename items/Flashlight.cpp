#include "../game.hpp"
#include "../map.hpp"
#include "Flashlight.hpp"
#include <sstream>

static bool step_forward_fl(const LabyrinthMap& map, size_t& x, size_t& y, Direction dir) {
	switch (dir) {
		case Direction::Left:  if (!map.can_move_left(x, y)) return false;  --x; return true;
		case Direction::Right: if (!map.can_move_right(x, y)) return false; ++x; return true;
		case Direction::Up:    if (!map.can_move_up(x, y)) return false;    --y; return true;
		case Direction::Down:  if (!map.can_move_down(x, y)) return false;  ++y; return true;
	}
	return false;
}

static const char* cell_name(CellContent c) {
	switch (c) {
		case CellContent::Empty: return "пусто";
		case CellContent::Treasure: return "сокровище";
		case CellContent::Hospital: return "больница";
		case CellContent::Arsenal: return "арсенал";
		case CellContent::Exit: return "выход";
	}
	return "пусто";
}

void Flashlight::apply(Game& game, LabyrinthMap& map, const std::string& playerName, Direction dir, std::vector<std::string>& messages) {
	auto ita = game.players.find(playerName);
	if (ita == game.players.end()) { messages.push_back("Нет такого игрока"); return; }

	size_t cx = ita->second.first, cy = ita->second.second;
	for (int i = 1; i <= 3; ++i) {
		if (!step_forward_fl(map, cx, cy, dir)) { messages.push_back("Фонарь: путь закрыт стеной"); break; }
		std::ostringstream os;
		os << "Фонарь " << i << ": " << cell_name(map.get_cell(cx, cy));
		// check players
		bool found_player = false;
		for (const auto& kv : game.players) {
			if (kv.first == playerName) continue;
			if (kv.second.first == cx && kv.second.second == cy) { found_player = true; break; }
		}
		if (found_player) os << " + игрок";
		messages.push_back(os.str());
	}
}


