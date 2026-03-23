#include "../game.hpp"
#include "../map.hpp"
#include "Flashlight.hpp"
#include "../generator.hpp"
#include "../rng.hpp"
#include <random>

static bool step_forward_fl(const LabyrinthMap& map, size_t& x, size_t& y, Direction dir) {
	switch (dir) {
		case Direction::Left:  if (!map.can_move_left(x, y)) return false;  --x; return true;
		case Direction::Right: if (!map.can_move_right(x, y)) return false; ++x; return true;
		case Direction::Up:    if (!map.can_move_up(x, y)) return false;    --y; return true;
		case Direction::Down:  if (!map.can_move_down(x, y)) return false;  ++y; return true;
	}
	return false;
}

static const char* cell_wire(CellContent c) {
	switch (c) {
		case CellContent::Empty: return "empty";
		case CellContent::Treasure: return "treasure";
		case CellContent::Hospital: return "hospital";
		case CellContent::Arsenal: return "arsenal";
		case CellContent::Exit: return "exit";
	}
	return "empty";
}

void Flashlight::apply(Game& game, LabyrinthMap& map, const std::string& playerName, Direction dir, Outcome& out) {
	auto ita = game.players.find(playerName);
	if (ita == game.players.end()) { out.logMessage(Message::InvalidTargetPlayer); return; }

	size_t cx = ita->second.first, cy = ita->second.second;
	for (int i = 1; i <= 3; ++i) {
		if (!step_forward_fl(map, cx, cy, dir)) {
			out.logMessage(Message::FlashlightBlocked, {dir_wire(dir)});
			break;
		}
		bool found_player = false;
		for (const auto& kv : game.players) {
			if (kv.first == playerName) continue;
			if (kv.second.first == cx && kv.second.second == cy) { found_player = true; break; }
		}
		const std::string cellTok = cell_wire(map.get_cell(cx, cy));
		if (found_player)
			out.logMessage(Message::FlashlightBeam, {dir_wire(dir), std::to_string(i), cellTok, "player"});
		else
			out.logMessage(Message::FlashlightBeam, {dir_wire(dir), std::to_string(i), cellTok});
	}
}

void Flashlight::onDepleted(Game& game, LabyrinthMap& map, const std::string& /*playerName*/, Outcome& out) {
	std::vector<std::pair<size_t,size_t>> empties;
	for (size_t y = 0; y < map.height; ++y) {
		for (size_t x = 0; x < map.width; ++x) {
			if (map.get_cell(x, y) == CellContent::Empty) empties.emplace_back(x, y);
		}
	}
	if (empties.empty()) return;
	std::mt19937 gen{rand_u32()};
	const auto pos = empties[game_rng::uniform_u32_below(gen, static_cast<uint32_t>(empties.size()))];
	long long key = (long long)pos.second * 1000000LL + (long long)pos.first;
	game.ground_items[key]["flashlight"] += 1;
	out.logMessage(Message::FlashlightDropped);
}

