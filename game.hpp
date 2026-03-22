#pragma once
#include "map.hpp"
#include "message.hpp"
#include "items.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

enum class Direction { Up, Down, Left, Right };

/** Сообщение о соседнем дыхании: `Game::move_player`, CLI `player-status` (JSON `messages` / `player_status`). */
inline const char* game_message_nearby_breathing() {
	return "Вы чувствуете чьё-то дыхание поблизости.";
}

inline const char* dir_ru(Direction d) {
	switch(d) {
		case Direction::Up:    return "вверх";
		case Direction::Down:  return "вниз";
		case Direction::Left:  return "влево";
		case Direction::Right: return "вправо";
	}
	return "";
}

struct Outcome {
    std::vector<std::string> messages;
    void logMessage(Message message) {
        messages.push_back(toString(message)); 
    }

    void logMessage(Message message, std::initializer_list<std::string> args) {
        auto loggedMessage = toString(message);
        for (auto& arg : args) {
            loggedMessage += ArgsSeparator;
            loggedMessage += arg;
        }
        messages.push_back(loggedMessage);
    }
};
struct MoveOutcome : Outcome {
	bool moved{false};
	std::pair<size_t,size_t> position{0,0};
};
struct AttackOutcome : Outcome {
	bool attacked{false};
	/** Для записи в лог replay: бот перенесён после удара игрока */
	bool bot_respawn_for_log{false};
	size_t bot_log_x{0}, bot_log_y{0};
};
struct UseOutcome : Outcome {
	bool used{false};
	std::vector<std::string> messages;
	bool bot_respawn_for_log{false};
	size_t bot_log_x{0}, bot_log_y{0};
};

/** Шаги бота для записи в replay-лог (история на карте). */
struct BotReplayStep {
	enum class Kind { Move, Kill } kind{Kind::Move};
	size_t x{0}, y{0};
	std::string victim;
};

struct Game {
	std::unordered_map<std::string, std::pair<size_t,size_t>> players;
	bool finished{false};

	bool enforce_turns{false};
	std::vector<std::string> turn_order;
	/** Состояние SplitMix64 для перемешивания очереди и вставки игроков; сериализуется (TURNRNG). */
	uint64_t turn_rng_state{0};
	size_t turn_index{0};
	int actions_per_turn{1};
	int actions_left{1};
	
	bool bot_enabled{false};
	size_t bot_x{0};
	size_t bot_y{0};
	int bot_steps_per_turn{1};

	void run_bot_turn(LabyrinthMap& map, Outcome& outcome, std::vector<BotReplayStep>* replay_log = nullptr);
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
	/** Привести turn_order к виду [живые игроки…, bot]; сохранить текущего актёра по имени. Вызывать после load. */
	void canonicalize_turn_order();
	MoveOutcome move_player(const std::string& name, Direction dir, LabyrinthMap& map);
	AttackOutcome attack(const std::string& name, Direction dir, LabyrinthMap& map);
	UseOutcome use_item(const std::string& name, const std::string& itemId, Direction dir, LabyrinthMap& map);

	/** Очистить перед use_item; выставляется при респавне бота после удара */
	bool pending_bot_respawn_log{false};
	size_t pending_bot_log_x{0}, pending_bot_log_y{0};
};

/** Игрок несёт сокровище (charges предмета `treasure` > 0). */
inline bool player_has_treasure(const Game& g, const std::string& name) {
	auto it = g.inventories.find(name);
	return it != g.inventories.end() && it->second.getCharges("treasure") > 0;
}

// Attempt to kill a victim (weapon attack). Returns true if hospitalized.
// If victim has armor, the armor absorbs the hit and the player survives.
bool attempt_kill(Game& game, LabyrinthMap& map, const std::string& victim, std::vector<std::string>& messages);

/** Удар по клетке (tx,ty): если там бот — перенос на случайную пустую клетку. */
bool hit_bot_at(Game& game, LabyrinthMap& map, size_t tx, size_t ty, std::vector<std::string>& messages);


