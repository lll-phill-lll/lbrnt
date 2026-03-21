#pragma once
#include "map.hpp"
#include "items.hpp"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

enum class Direction { Up, Down, Left, Right };

inline const char* dir_ru(Direction d) {
	switch(d) {
		case Direction::Up:    return "вверх";
		case Direction::Down:  return "вниз";
		case Direction::Left:  return "влево";
		case Direction::Right: return "вправо";
	}
	return "";
}

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

/** Шаги бота для записи в replay-лог (история на карте). */
struct BotReplayStep {
	enum class Kind { Move, Kill } kind{Kind::Move};
	size_t x{0}, y{0};
	std::string victim;
};

struct Game {
	std::unordered_map<std::string, std::pair<size_t,size_t>> players;
	std::unordered_set<std::string> players_with_treasure;
	bool finished{false};

	bool enforce_turns{false};
	std::vector<std::string> turn_order;
	size_t turn_index{0};
	int actions_per_turn{1};
	int actions_left{1};
	
	bool bot_enabled{false};
	size_t bot_x{0};
	size_t bot_y{0};
	int bot_steps_per_turn{1};

	void run_bot_turn(LabyrinthMap& map, std::vector<std::string>& messages,
		std::vector<BotReplayStep>* replay_log = nullptr);
	/** Только для replay: убийство ботом (как в run_bot_turn, без проверки брони). */
	void apply_replay_bot_kill(const std::string& victim, LabyrinthMap& map);
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
	void init_turns();
	MoveOutcome move_player(const std::string& name, Direction dir, LabyrinthMap& map);
	AttackOutcome attack(const std::string& name, Direction dir, LabyrinthMap& map);
	UseOutcome use_item(const std::string& name, const std::string& itemId, Direction dir, LabyrinthMap& map);
};

// Attempt to kill a victim (weapon attack). Returns true if hospitalized.
// If victim has armor, the armor absorbs the hit and the player survives.
bool attempt_kill(Game& game, LabyrinthMap& map, const std::string& victim, std::vector<std::string>& messages);


