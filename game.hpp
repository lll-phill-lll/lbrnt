#pragma once
#include "map.hpp"
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
	// inventory: broken knife set; if name is in set -> knife broken, else active
	std::unordered_set<std::string> broken_knife;
	// stable per-player color hex (e.g., "#1f77b4")
	std::unordered_map<std::string, std::string> player_color;
	// ground loot: treasure count per cell key = y*1e6 + x
	std::unordered_map<long long, int> loot_treasure;
	// per-player item charges: itemId -> remaining charges
	std::unordered_map<std::string, std::unordered_map<std::string,int>> item_charges;
	// ground items: per cell -> map of itemId to charges granted on pickup
	std::unordered_map<long long, std::unordered_map<std::string,int>> ground_items;

	bool add_player(const std::string& name, std::pair<size_t,size_t> at, const LabyrinthMap& map, std::string& err);
	MoveOutcome move_player(const std::string& name, Direction dir, LabyrinthMap& map);
	AttackOutcome attack(const std::string& name, Direction dir, LabyrinthMap& map);
	// Generic item usage: itemId in {"knife","shotgun","rifle","flashlight"}
	UseOutcome use_item(const std::string& name, const std::string& itemId, Direction dir, LabyrinthMap& map);
};


