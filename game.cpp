#include "game.hpp"
#include "items/Item.hpp"
#include "items/Knife.hpp"
#include "items/Shotgun.hpp"
#include "items/Rifle.hpp"
#include "items/Flashlight.hpp"
#include "items/LootTreasure.hpp"
#include "locations/Location.hpp"
#include "generator.hpp"
#include "locations/Hospital.hpp"
#include "rng.hpp"
#include <algorithm>
#include <memory>
#include <queue>

static size_t manhattan(std::pair<size_t,size_t> a, std::pair<size_t,size_t> b) {
	size_t dx = a.first > b.first ? a.first - b.first : b.first - a.first;
	size_t dy = a.second > b.second ? a.second - b.second : b.second - a.second;
	return dx + dy;
}

static void ensure_turns_initialized(Game& g) {
	if (!g.enforce_turns) return;
	if (!g.turn_order.empty()) return;
	if (g.turn_rng_state == 0)
		g.turn_rng_state = game_rng::splitmix64(0xA5A5A5A5A5A5A5A5ull);
	std::vector<std::string> names;
	names.reserve(g.players.size());
	for (const auto& kv : g.players) names.push_back(kv.first);
	// Порядок обхода unordered_map не задан — фиксируем вход перестановки.
	std::sort(names.begin(), names.end());
	// Fisher–Yates с сохраняемым turn_rng_state (SplitMix64).
	for (size_t i = names.size(); i > 1; --i) {
		size_t j = game_rng::uniform_exclusive(g.turn_rng_state, i);
		std::swap(names[j], names[i - 1]);
	}
	g.turn_order = std::move(names);
	// Бот только в конце очереди — при turn_index=0 первым всегда живой игрок, не бот.
	if (g.bot_enabled) g.turn_order.push_back("bot");
	g.turn_index = 0;
	g.actions_left = std::max(1, g.actions_per_turn);
}
static bool is_players_turn(Game& g, const std::string& name) {
	if (!g.enforce_turns) return true;
	ensure_turns_initialized(g);
	if (g.turn_order.empty()) return true;
	return g.turn_order[g.turn_index] == name;
}
static void advance_turn(Game& g, LabyrinthMap& map) {
	(void)map;
	if (!g.enforce_turns) return;
	if (g.turn_order.empty()) return;
	// Нормализованная очередь: только живые игроки (порядок как в turn_order) + бот всегда в конце.
	// Так не остаётся «мёртвых» имён и не нужна отдельная логика «после бота не брать бота».
	std::vector<std::string> filtered;
	filtered.reserve(g.turn_order.size());
	for (const auto& n : g.turn_order) {
		if (n == "bot") continue;
		if (g.players.count(n)) filtered.push_back(n);
	}
	if (g.bot_enabled) filtered.push_back("bot");
	if (filtered.empty()) { g.turn_order.clear(); g.turn_index = 0; return; }

	std::string cur_name;
	if (g.turn_index < g.turn_order.size()) cur_name = g.turn_order[g.turn_index];
	size_t cur_idx = 0;
	{
		bool found = false;
		for (size_t i = 0; i < filtered.size(); ++i) {
			if (filtered[i] == cur_name) {
				cur_idx = i;
				found = true;
				break;
			}
		}
		if (!found)
			cur_idx = filtered.size() - 1; // «как после бота» → следующий шаг даст первого человека
	}

	std::string next_name;
	for (size_t step = 1; step <= filtered.size(); ++step) {
		size_t j = (cur_idx + step) % filtered.size();
		const std::string& cand = filtered[j];
		if (cand == "bot") {
			if (g.bot_enabled) {
				next_name = cand;
				break;
			}
			continue;
		}
		next_name = cand;
		break;
	}

	size_t next_idx = 0;
	if (!next_name.empty()) {
		auto it = std::find(filtered.begin(), filtered.end(), next_name);
		if (it != filtered.end()) next_idx = static_cast<size_t>(it - filtered.begin());
	}
	g.turn_order = std::move(filtered);
	g.turn_index = next_idx % g.turn_order.size();
	g.actions_left = std::max(1, g.actions_per_turn);
}
static void consume_action_or_advance(Game& g, LabyrinthMap& map) {
	if (!g.enforce_turns) return;
	if (g.actions_left > 0) g.actions_left -= 1;
	if (g.actions_left <= 0) advance_turn(g, map);
}
void Game::init_turns() {
	if (!enforce_turns) return;
	// Всегда пересобираем очередь (иначе повторный init-turns — no-op при непустом turn_order,
	// и сценарий из «Зафиксировать начало» расходится с pytest по тому же scenario.json).
	turn_order.clear();
	turn_index = 0;
	ensure_turns_initialized(*this);
}

void Game::canonicalize_turn_order() {
	if (!enforce_turns) return;
	std::string current_name;
	if (!turn_order.empty() && turn_index < turn_order.size())
		current_name = turn_order[turn_index];
	std::vector<std::string> filtered;
	for (const auto& n : turn_order) {
		if (n == "bot") continue;
		if (players.count(n)) filtered.push_back(n);
	}
	if (bot_enabled) filtered.push_back("bot");
	if (filtered.empty()) {
		turn_order.clear();
		turn_index = 0;
		return;
	}
	turn_order = std::move(filtered);
	turn_index = 0;
	for (size_t i = 0; i < turn_order.size(); ++i) {
		if (turn_order[i] == current_name) {
			turn_index = i;
			return;
		}
	}
	// Устаревшее имя в очереди — с начала (первый живой игрок, если есть)
	turn_index = 0;
}

bool Game::add_player(const std::string& name, std::pair<size_t,size_t> at, const LabyrinthMap& map, std::string& err) {
	if (!map.in_bounds(static_cast<long>(at.first), static_cast<long>(at.second))) {
		err = "Координаты вне карты";
		return false;
	}
	players[name] = at;
	// initially knife is active
	broken_knife.erase(name);
	// initially only knife is available: 1 charge
	inventories[name].setCharges("knife", 1);
	// if turn order already exists — случайная позиция среди людей (детерминированно от turn_rng_state)
	if (enforce_turns && !turn_order.empty()) {
		if (turn_rng_state == 0)
			turn_rng_state = game_rng::splitmix64(0xA5A5A5A5A5A5A5A5ull);
		std::vector<std::string> human;
		human.reserve(turn_order.size());
		for (const auto& n : turn_order) {
			if (n != "bot") human.push_back(n);
		}
		const size_t slots = human.size() + 1;
		size_t pos = game_rng::uniform_exclusive(turn_rng_state, slots);
		human.insert(human.begin() + static_cast<decltype(human)::difference_type>(pos), name);
		turn_order.clear();
		turn_order.insert(turn_order.end(), human.begin(), human.end());
		if (bot_enabled) turn_order.push_back("bot");
	}
	// ensure bot present in order if enabled
	if (enforce_turns && bot_enabled) {
		if (std::find(turn_order.begin(), turn_order.end(), std::string("bot")) == turn_order.end()) {
			turn_order.push_back("bot");
		}
	}
	// assign stable color if not set
	if (!player_color.count(name)) {
		static const char* palette[] = {
			"#1f77b4","#ff7f0e","#2ca02c","#d62728","#9467bd",
			"#8c564b","#e377c2","#17becf","#bcbd22","#7f7f7f"
		};
		std::unordered_set<std::string> used;
		for (const auto& kv : player_color) used.insert(kv.second);
		const char* chosen = palette[0];
		for (const char* c : palette) {
			if (!used.count(c)) { chosen = c; break; }
		}
		player_color[name] = chosen;
	}
	return true;
}

static long long key_xy(size_t x, size_t y) {
	return (long long)y * 1000000LL + (long long)x;
}

/** Сбросить весь carried treasure в кучу loot_treasure на клетке. */
static void drop_carried_treasure_on_ground(Game& g, const std::string& victim, size_t x, size_t y) {
	auto itInv = g.inventories.find(victim);
	if (itInv == g.inventories.end()) return;
	int t = itInv->second.getCharges("treasure");
	if (t <= 0) return;
	itInv->second.removeItem("treasure");
	g.loot_treasure[key_xy(x, y)] += t;
}

MoveOutcome Game::move_player(const std::string& name, Direction dir, LabyrinthMap& map) {
	MoveOutcome out;
	auto it = players.find(name);
	if (it == players.end()) {
		out.logMessage(Message::InvalidTargetPlayer);
		return out;
	}
	if (!is_players_turn(*this, name)) {
		out.logMessage(Message::NotYourMove);
		return out;
	}
	auto pos = it->second;
	long nx = static_cast<long>(pos.first);
	long ny = static_cast<long>(pos.second);
	bool can = false;
	switch (dir) {
		case Direction::Left:  can = map.can_move_left(pos.first, pos.second);  nx -= 1; break;
		case Direction::Right: can = map.can_move_right(pos.first, pos.second); nx += 1; break;
		case Direction::Up:    can = map.can_move_up(pos.first, pos.second);    ny -= 1; break;
		case Direction::Down:  can = map.can_move_down(pos.first, pos.second);  ny += 1; break;
	}
	// Exiting via border exit edge
	if (!map.in_bounds(nx, ny)) {
		// Determine edge being crossed
		bool through_exit = false;
		if (nx < 0) {
			through_exit = map.has_exit && map.exit_vertical && map.exit_x == 0 && map.exit_y == pos.second && !map.v_walls[pos.second][0];
		} else if (static_cast<size_t>(nx) >= map.width) {
			through_exit = map.has_exit && map.exit_vertical && map.exit_x == map.width && map.exit_y == pos.second && !map.v_walls[pos.second][map.width];
		} else if (ny < 0) {
			through_exit = map.has_exit && !map.exit_vertical && map.exit_y == 0 && map.exit_x == pos.first && !map.h_walls[0][pos.first];
		} else if (static_cast<size_t>(ny) >= map.height) {
			through_exit = map.has_exit && !map.exit_vertical && map.exit_y == map.height && map.exit_x == pos.first && !map.h_walls[map.height][pos.first];
		}
		if (through_exit) {
			// Exit the maze
			out.moved = true;
			out.position = pos; // remain at border cell
			if (player_has_treasure(*this, name)) {
				out.logMessage(Message::ExitFoundWithTreasure);
				finished = true;
			} else {
				out.logMessage(Message::ExitFoundWithoutTreasure);
			}
			consume_action_or_advance(*this, map);
			return out;
		}
	}
	if (!can) {
		out.moved = false;
		out.position = pos;
		bool outer = false;
		switch (dir) {
			case Direction::Left:  outer = (pos.first == 0); break;
			case Direction::Right: outer = (pos.first + 1 >= map.width); break;
			case Direction::Up:    outer = (pos.second == 0); break;
			case Direction::Down:  outer = (pos.second + 1 >= map.height); break;
		}
        if (outer) {
            out.logMessage(Message::OuterWallCrash, {dir_ru(dir)});
        } else {
            out.logMessage(Message::InnerWallCrash, {dir_ru(dir)});
        }
		if (enforce_turns) { actions_left = 0; advance_turn(*this, map); }
		return out;
	}
	std::pair<size_t,size_t> new_pos{static_cast<size_t>(nx), static_cast<size_t>(ny)};
	// onExit for previous location if leaving it
	CellContent prevCell = map.get_cell(pos.first, pos.second);
	players[name] = new_pos;
	out.moved = true;
	out.position = new_pos;
    out.logMessage(Message::Moved, {dir_ru(dir)});

	CellContent newCell = map.get_cell(new_pos.first, new_pos.second);
	// If moved between different location types, call onExit for previous
	if (prevCell != newCell) {
		auto* locPrev = getLocationFor(prevCell);
		locPrev->onExit(*this, map, name, pos.first, pos.second, out.messages);
	}

	// Generic onEnter for any cell
	{
		auto* locNew = getLocationFor(newCell);
		locNew->onEnter(*this, map, name, new_pos.first, new_pos.second, out.messages);
	}

	switch (newCell) {
		default: break;
	}
	// Pick up loot treasure if present
	auto lk = key_xy(new_pos.first, new_pos.second);
	auto itloot = loot_treasure.find(lk);
	if (itloot != loot_treasure.end() && itloot->second > 0) {
		int cur = inventories[name].getCharges("treasure");
		inventories[name].setCharges("treasure", cur + 1);
		itloot->second -= 1;
		if (itloot->second <= 0) loot_treasure.erase(itloot);
		out.logMessage(Message::TreasureFound);
	}
	// Pick up ground items if present
	auto itItems = ground_items.find(lk);
	if (itItems != ground_items.end() && !itItems->second.empty()) {
		for (const auto& kv : itItems->second) {
			const std::string& itemId = kv.first;
			int grant = kv.second;
			if (grant <= 0) continue;
			int cur = inventories[name].getCharges(itemId);
			inventories[name].setCharges(itemId, cur + grant);
			if (itemId == "knife" && inventories[name].getCharges("knife") > 0) broken_knife.erase(name);
			if (itemId == "flashlight") out.logMessage(Message::FlashLightFound);
			else if (itemId == "rifle") out.logMessage(Message::RifleFound);
			else if (itemId == "shotgun") out.logMessage(Message::ShotgunFound);
			else if (itemId == "knife") out.logMessage(Message::KnifeFound);
			else if (itemId == "armor") out.logMessage(Message::ArmourFound);
			else if (itemId == "treasure") out.logMessage(Message::TreasurePicked);
            else out.logMessage(Message::ItemFound, {itemId});
		}
		ground_items.erase(itItems);
	}
	auto adjacentBreathingVisible = [&](size_t ox, size_t oy) -> bool {
		if (ox + 1 == new_pos.first && oy == new_pos.second) return map.can_move_left(new_pos.first, new_pos.second);
		if (ox == new_pos.first + 1 && oy == new_pos.second) return map.can_move_right(new_pos.first, new_pos.second);
		if (oy + 1 == new_pos.second && ox == new_pos.first) return map.can_move_up(new_pos.first, new_pos.second);
		if (oy == new_pos.second + 1 && ox == new_pos.first) return map.can_move_down(new_pos.first, new_pos.second);
		return false;
	};
	bool feltBreathing = false;
	for (const auto& kv : players) {
		if (kv.first == name) continue;
		auto other = kv.second;
		size_t dx = (new_pos.first > other.first) ? (new_pos.first - other.first) : (other.first - new_pos.first);
		size_t dy = (new_pos.second > other.second) ? (new_pos.second - other.second) : (other.second - new_pos.second);
		if (dx + dy != 1) continue; // only adjacent, not same cell
		if (adjacentBreathingVisible(other.first, other.second)) {
			feltBreathing = true;
			break;
		}
	}
	if (!feltBreathing && bot_enabled) {
		size_t bx = bot_x, by = bot_y;
		size_t dx = (new_pos.first > bx) ? (new_pos.first - bx) : (bx - new_pos.first);
		size_t dy = (new_pos.second > by) ? (new_pos.second - by) : (by - new_pos.second);
		if (dx + dy == 1 && adjacentBreathingVisible(bx, by)) feltBreathing = true;
	}
	if (feltBreathing) out.logMessage(Message::Breathe);
	consume_action_or_advance(*this, map);
	return out;
}

AttackOutcome Game::attack(const std::string& name, Direction dir, LabyrinthMap& map) {
	// Backward compatibility: attack = use knife
	AttackOutcome out;
	if (!is_players_turn(*this, name)) {
        out.logMessage(Message::NotYourMove);
		return out;
	}
	auto use = use_item(name, std::string("knife"), dir, map);
	out.attacked = use.used;
	out.messages = std::move(use.messages);
	out.bot_respawn_for_log = use.bot_respawn_for_log;
	out.bot_log_x = use.bot_log_x;
	out.bot_log_y = use.bot_log_y;
	return out;
}

UseOutcome Game::use_item(const std::string& name, const std::string& itemId, Direction dir, LabyrinthMap& map) {
	UseOutcome out;
	pending_bot_respawn_log = false;
	auto itp = players.find(name);
	if (itp == players.end()) { out.logMessage(Message::InvalidTargetPlayer); return out; }
	if (!is_players_turn(*this, name)) {
		out.logMessage(Message::NotYourMove);
		return out;
	}
	// Create requested item
	std::unique_ptr<Item> item;
	if (itemId == "knife") item = std::make_unique<Knife>();
	else if (itemId == "shotgun") item = std::make_unique<Shotgun>();
	else if (itemId == "rifle") item = std::make_unique<Rifle>();
	else if (itemId == "flashlight") item = std::make_unique<Flashlight>();
	else if (itemId == "treasure") item = std::make_unique<LootTreasure>();
	else { out.logMessage(Message::UnknownItem); return out; }
	// Delegate charge logic to item
	extern bool item_use(Game& game, Item& item, LabyrinthMap& map, const std::string& playerName, Direction dir, std::vector<std::string>& messages);
	out.used = item_use(*this, *item, map, name, dir, out.messages);
	if (pending_bot_respawn_log) {
		out.bot_respawn_for_log = true;
		out.bot_log_x = pending_bot_log_x;
		out.bot_log_y = pending_bot_log_y;
	}
	if (out.used) {
		// terminating items: knife, rifle, shotgun
		std::string id = item->id();
		if (id == "knife" || id == "rifle" || id == "shotgun") {
			actions_left = 0;
			advance_turn(*this, map);
		} else {
			consume_action_or_advance(*this, map);
		}
	}
	return out;
}

void Game::apply_replay_bot_kill(const std::string& victim, LabyrinthMap& map) {
	auto itp = players.find(victim);
	if (itp == players.end()) return;
	drop_carried_treasure_on_ground(*this, victim, itp->second.first, itp->second.second);
	if (auto* loc = getLocationFor(CellContent::Hospital)) {
		if (auto* hosp = dynamic_cast<HospitalLocation*>(loc))
			hosp->teleportToHospital(*this, map, victim);
	}
}

static bool player_stands_on_hospital(const Game& g, const LabyrinthMap& map, const std::string& name) {
	auto it = g.players.find(name);
	if (it == g.players.end()) return false;
	return map.get_cell(it->second.first, it->second.second) == CellContent::Hospital;
}

static bool try_random_bot_step(Game& g, LabyrinthMap& map, std::vector<BotReplayStep>* replay_log) {
	size_t bx = g.bot_x, by = g.bot_y;
	int ord[4] = {0, 1, 2, 3};
	for (int i = 3; i > 0; --i) {
		int j = static_cast<int>(rand_u32() % static_cast<unsigned>(i + 1));
		std::swap(ord[i], ord[j]);
	}
	for (int k = 0; k < 4; ++k) {
		long nx = static_cast<long>(bx), ny = static_cast<long>(by);
		bool ok = false;
		switch (ord[k]) {
			case 0: ok = map.can_move_left(bx, by); nx -= 1; break;
			case 1: ok = map.can_move_right(bx, by); nx += 1; break;
			case 2: ok = map.can_move_up(bx, by); ny -= 1; break;
			case 3: ok = map.can_move_down(bx, by); ny += 1; break;
		}
		if (ok && map.in_bounds(nx, ny)) {
			g.bot_x = static_cast<size_t>(nx);
			g.bot_y = static_cast<size_t>(ny);
			if (replay_log) {
				BotReplayStep s;
				s.kind = BotReplayStep::Kind::Move;
				s.x = g.bot_x;
				s.y = g.bot_y;
				replay_log->push_back(s);
			}
			return true;
		}
	}
	return false;
}

// Бот: n = bot_steps_per_turn шагов за ход. Удар возможен только если кратчайший путь к дистанции удара ≤ n−1;
// если кратчайший путь ровно n — до цели дойти можно, удара нет. Удар до исчерпания n шагов — ход сразу кончается.
void Game::run_bot_turn(LabyrinthMap& map, Outcome& outcome, std::vector<BotReplayStep>* replay_log) {
	if (!bot_enabled) { advance_turn(*this, map); return; }
	if (players.empty()) { advance_turn(*this, map); return; }
	if (bot_x >= map.width || bot_y >= map.height) { advance_turn(*this, map); return; }

	const size_t n = static_cast<size_t>(std::max(1, bot_steps_per_turn));
	const size_t sx = bot_x, sy = bot_y;
	const size_t INF = map.width * map.height + 5;

	auto try_kill_victim = [&](const std::string& victim) -> bool {
		auto itp = players.find(victim);
		if (itp == players.end()) return false;
		drop_carried_treasure_on_ground(*this, victim, itp->second.first, itp->second.second);
		if (auto* loc = getLocationFor(CellContent::Hospital)) {
			if (auto* hosp = dynamic_cast<HospitalLocation*>(loc)) {
				if (hosp->teleportToHospital(*this, map, victim)) {
					outcome.logPlayerMessage(victim, Message::KilledByBot, {victim});
					if (replay_log) {
						BotReplayStep ks;
						ks.kind = BotReplayStep::Kind::Kill;
						ks.victim = victim;
						replay_log->push_back(ks);
					}
					return true;
				}
			}
		}
		return false;
	};

	auto try_kill_adjacent = [&]() -> bool {
		std::vector<std::string> adj;
		for (const auto& kv : players) {
			if (player_stands_on_hospital(*this, map, kv.first)) continue;
			if (manhattan({bot_x, bot_y}, kv.second) <= 1) adj.push_back(kv.first);
		}
		if (adj.empty()) return false;
		const std::string& victim = adj[static_cast<size_t>(rand_u32() % adj.size())];
		return try_kill_victim(victim);
	};

	bool any_outside_hospital = false;
	for (const auto& kv : players) {
		if (!player_stands_on_hospital(*this, map, kv.first)) {
			any_outside_hospital = true;
			break;
		}
	}
	if (!any_outside_hospital) {
		for (size_t s = 0; s < n; ++s) {
			if (!try_random_bot_step(*this, map, replay_log)) break;
		}
		outcome.logMessage(Message::BotMoved);
		actions_left = 0;
		advance_turn(*this, map);
		return;
	}

	std::vector<std::vector<size_t>> dist(map.height, std::vector<size_t>(map.width, INF));
	std::vector<std::vector<std::pair<int, int>>> prev(map.height, std::vector<std::pair<int, int>>(map.width, {-1, -1}));
	std::queue<std::pair<size_t, size_t>> q;
	dist[sy][sx] = 0;
	q.push({sx, sy});
	while (!q.empty()) {
		std::pair<size_t, size_t> curp = q.front();
		q.pop();
		size_t cx = curp.first, cy = curp.second;
		auto relax = [&](size_t nx, size_t ny, bool can) {
			if (!can || dist[ny][nx] != INF) return;
			dist[ny][nx] = dist[cy][cx] + 1;
			prev[ny][nx] = {static_cast<int>(cx), static_cast<int>(cy)};
			q.push({nx, ny});
		};
		if (cx > 0) relax(cx - 1, cy, map.can_move_left(cx, cy));
		if (cx + 1 < map.width) relax(cx + 1, cy, map.can_move_right(cx, cy));
		if (cy > 0) relax(cx, cy - 1, map.can_move_up(cx, cy));
		if (cy + 1 < map.height) relax(cx, cy + 1, map.can_move_down(cx, cy));
	}

	size_t bestD = INF;
	std::pair<size_t, size_t> bestCell{0, 0};
	std::string bestName;

	for (const auto& kv : players) {
		if (player_stands_on_hospital(*this, map, kv.first)) continue;
		size_t px = kv.second.first, py = kv.second.second;
		std::vector<std::pair<size_t, size_t>> cand;
		cand.push_back({px, py});
		if (px > 0) cand.push_back({px - 1, py});
		if (px + 1 < map.width) cand.push_back({px + 1, py});
		if (py > 0) cand.push_back({px, py - 1});
		if (py + 1 < map.height) cand.push_back({px, py + 1});
		for (const auto& c : cand) {
			if (c.first >= map.width || c.second >= map.height) continue;
			size_t d = dist[c.second][c.first];
			if (d == INF) continue;
			if (d < bestD || (d == bestD && kv.first < bestName)) {
				bestD = d;
				bestCell = c;
				bestName = kv.first;
			}
		}
	}
	(void)bestName;

	if (bestD == INF) {
		for (size_t s = 0; s < n; ++s) {
			if (!try_random_bot_step(*this, map, replay_log)) break;
		}
        outcome.logMessage(Message::BotMoved);
		actions_left = 0;
		advance_turn(*this, map);
		return;
	}

	// d = 0: уже в зоне удара; удар только если 0 ≤ n−1 (n ≥ 1)
	if (bestD == 0 && bestD <= n - 1) {
		try_kill_adjacent();
        outcome.logMessage(Message::BotMoved);
		actions_left = 0;
		advance_turn(*this, map);
		return;
	}

	std::vector<std::pair<size_t, size_t>> path_cells;
	{
		auto cur = bestCell;
		while (!(cur.first == sx && cur.second == sy)) {
			path_cells.push_back(cur);
			auto p = prev[cur.second][cur.first];
			if (p.first < 0) break;
			cur = {static_cast<size_t>(p.first), static_cast<size_t>(p.second)};
		}
		std::reverse(path_cells.begin(), path_cells.end());
	}

	size_t moves_used = 0;
	for (size_t i = 0; i < path_cells.size() && moves_used < n; ++i) {
		bot_x = path_cells[i].first;
		bot_y = path_cells[i].second;
		moves_used++;
		if (replay_log) {
			BotReplayStep s;
			s.kind = BotReplayStep::Kind::Move;
			s.x = bot_x;
			s.y = bot_y;
			replay_log->push_back(s);
		}
		if (moves_used == bestD && bestD <= n - 1) {
			try_kill_adjacent();
            outcome.logMessage(Message::BotMoved);
			actions_left = 0;
			advance_turn(*this, map);
			return;
		}
	}

    outcome.logMessage(Message::BotMoved);
	actions_left = 0;
	advance_turn(*this, map);
}

bool hit_bot_at(Game& game, LabyrinthMap& map, size_t tx, size_t ty, std::vector<std::string>& messages) {
	if (!game.bot_enabled) return false;
	if (tx != game.bot_x || ty != game.bot_y) return false;
	std::vector<std::pair<size_t, size_t>> spots;
	for (size_t y = 0; y < map.height; ++y) {
		for (size_t x = 0; x < map.width; ++x) {
			if (map.get_cell(x, y) != CellContent::Empty) continue;
			bool occupied = false;
			for (const auto& kv : game.players) {
				if (kv.second.first == x && kv.second.second == y) {
					occupied = true;
					break;
				}
			}
			if (occupied) continue;
			spots.emplace_back(x, y);
		}
	}
	if (spots.empty()) {
		appendWire(messages, Message::BotStays);
		game.pending_bot_respawn_log = false;
		return true;
	}
	const size_t pick = static_cast<size_t>(rand_u32() % spots.size());
	game.bot_x = spots[pick].first;
	game.bot_y = spots[pick].second;
	appendWire(messages, Message::BotDestroyedRelocated);
	game.pending_bot_respawn_log = true;
	game.pending_bot_log_x = game.bot_x;
	game.pending_bot_log_y = game.bot_y;
	return true;
}

bool attempt_kill(Game& game, LabyrinthMap& map, const std::string& victim, std::vector<std::string>& messages) {
	auto itp = game.players.find(victim);
	if (itp == game.players.end()) return false;

	// Check armor
	auto itInv = game.inventories.find(victim);
	if (itInv != game.inventories.end()) {
		int armor = itInv->second.getCharges("armor");
		if (armor > 0) {
			armor -= 1;
			if (armor <= 0)
				itInv->second.removeItem("armor");
			else
				itInv->second.setCharges("armor", armor);
			appendWire(messages, Message::ArmorAbsorbedHit, {victim});
			return false;
		}
	}

	auto pos = itp->second;
	drop_carried_treasure_on_ground(game, victim, pos.first, pos.second);

	bool sent = false;
	if (auto* loc = getLocationFor(CellContent::Hospital)) {
		if (auto* hosp = dynamic_cast<HospitalLocation*>(loc)) {
			sent = hosp->teleportToHospital(game, map, victim);
		}
	}
	if (!sent) appendWire(messages, Message::HospitalNotFound);
	return sent;
}
