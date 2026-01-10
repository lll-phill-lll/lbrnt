#pragma once
#include "map.hpp"
#include "items.hpp"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

enum class Direction { Up, Down, Left, Right };

struct MoveOutcome {
	bool moved{false};
	std::pair<size_t,size_t> position{0,0};
	std::vector<std::string> messages;
};
struct AttackOutcome {
	bool attacked{false};
	std::vector<std::string> messages;
};
struct UseOutcome {
	bool used{false};
	std::vector<std::string> messages;
};

struct Game {
	std::unordered_map<std::string, std::pair<size_t,size_t>> players;
	std::unordered_set<std::string> players_with_treasure;
	bool finished{false};
	// Turn order
	bool enforce_turns{false};
	std::vector<std::string> turn_order; // fixed order when enforced
	size_t turn_index{0};                 // index in turn_order
	// Action points (per turn and remaining for current actor)
	int actions_per_turn{1};
	int actions_left{1};
	// Special bot
	bool bot_enabled{false};
	size_t bot_x{0};
	size_t bot_y{0};
	int bot_steps_per_turn{1};

	// Bot AI turn
	void run_bot_turn(LabyrinthMap& map, std::vector<std::string>& messages);
	// inventory: broken knife set; if name is in set -> knife broken, else active
	std::unordered_set<std::string> broken_knife;
	// stable per-player color hex (e.g., "#1f77b4")
	std::unordered_map<std::string, std::string> player_color;
	// ground loot: treasure count per cell key = y*1e6 + x
	std::unordered_map<long long, int> loot_treasure;
	// per-player inventory
	std::unordered_map<std::string, Inventory> inventories;
	// ground items: per cell -> map of itemId to charges granted on pickup
	std::unordered_map<long long, std::unordered_map<std::string,int>> ground_items;

	bool add_player(const std::string& name, std::pair<size_t,size_t> at, const LabyrinthMap& map, std::string& err);
	MoveOutcome move_player(const std::string& name, Direction dir, LabyrinthMap& map);
	AttackOutcome attack(const std::string& name, Direction dir, LabyrinthMap& map);
	// Generic item usage: itemId in {"knife","shotgun","rifle","flashlight"}
	UseOutcome use_item(const std::string& name, const std::string& itemId, Direction dir, LabyrinthMap& map);
};


