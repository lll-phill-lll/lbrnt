#pragma once
#include "map.hpp"
#include "game.hpp"
#include <string>
#include <vector>

enum class LogType { Move, Attack, UseItem, AddPlayer, AddPlayerRandom };
struct LogEntry {
	LogType type{LogType::Move};
	std::string name;
	Direction dir{Direction::Up};
	size_t x{0}, y{0};
	std::string item;             // for UseItem
};

struct AppState {
	LabyrinthMap map;
	Game game;
	std::vector<LogEntry> log;
	LabyrinthMap base_map;
	Game base_game;
	// RNG state: seed is set once at generate; nonce increments on each random draw
	unsigned int random_seed{0};
	unsigned long long random_nonce{0};

	static bool save(const AppState& st, const std::string& path, std::string& err);
	static bool load(AppState& st, const std::string& path, std::string& err);
	void set_base_from_current() {
		base_map = map;
		base_game = game;
	}
};


