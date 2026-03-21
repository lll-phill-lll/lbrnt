#include "game.hpp"
#include "items/Item.hpp"
#include "items/Knife.hpp"
#include "items/Shotgun.hpp"
#include "items/Rifle.hpp"
#include "items/Flashlight.hpp"
#include "locations/Location.hpp"
#include "generator.hpp"
#include "locations/Hospital.hpp"
#include <algorithm>
#include <memory>
#include <random>
#include <queue>

static size_t manhattan(std::pair<size_t,size_t> a, std::pair<size_t,size_t> b) {
	size_t dx = a.first > b.first ? a.first - b.first : b.first - a.first;
	size_t dy = a.second > b.second ? a.second - b.second : b.second - a.second;
	return dx + dy;
}

static void ensure_turns_initialized(Game& g) {
	if (!g.enforce_turns) return;
	if (!g.turn_order.empty()) return;
	std::vector<std::string> names;
	names.reserve(g.players.size());
	for (const auto& kv : g.players) names.push_back(kv.first);
	std::mt19937 gen{rand_u32()};
	std::shuffle(names.begin(), names.end(), gen);
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
	// if turn order already exists, insert new player at random position
	if (enforce_turns && !turn_order.empty()) {
		std::mt19937 gen{rand_u32()};
		std::uniform_int_distribution<size_t> dist(0, turn_order.size());
		size_t pos = dist(gen);
		turn_order.insert(turn_order.begin() + pos, name);
		// ensure bot remains last
		if (bot_enabled) {
			auto it = std::find(turn_order.begin(), turn_order.end(), std::string("bot"));
			if (it != turn_order.end() && (it + 1) != turn_order.end()) {
				turn_order.erase(it);
				turn_order.push_back("bot");
			}
		}
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

MoveOutcome Game::move_player(const std::string& name, Direction dir, LabyrinthMap& map) {
	MoveOutcome out;
	auto it = players.find(name);
	if (it == players.end()) {
		out.messages.push_back("Нет такого игрока");
		return out;
	}
	if (!is_players_turn(*this, name)) {
		out.messages.push_back("Сейчас не ваш ход");
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
			if (players_with_treasure.count(name)) {
				out.messages.push_back("Выход найден на внешней стене! Игрок вынес сокровище!");
				finished = true;
			} else {
				out.messages.push_back("Выход найден на внешней стене, но без сокровища.");
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
		out.messages.push_back(std::string(outer ? "Врезался во внешнюю стену (" : "Врезался в стену (") + dir_ru(dir) + ")");
		if (enforce_turns) { actions_left = 0; advance_turn(*this, map); }
		return out;
	}
	std::pair<size_t,size_t> new_pos{static_cast<size_t>(nx), static_cast<size_t>(ny)};
	// onExit for previous location if leaving it
	CellContent prevCell = map.get_cell(pos.first, pos.second);
	players[name] = new_pos;
	out.moved = true;
	out.position = new_pos;
	out.messages.push_back(std::string("Прошёл ") + dir_ru(dir));

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
		players_with_treasure.insert(name);
		itloot->second -= 1;
		if (itloot->second <= 0) loot_treasure.erase(itloot);
		out.messages.push_back("Поднято сокровище с земли.");
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
			if (itemId == "flashlight") out.messages.push_back("Подобран фонарь");
			else if (itemId == "rifle") out.messages.push_back("Подобрано ружьё");
			else if (itemId == "shotgun") out.messages.push_back("Подобран дробовик");
			else if (itemId == "knife") out.messages.push_back("Подобран нож");
			else if (itemId == "armor") out.messages.push_back("Подобрана броня");
			else out.messages.push_back("Подобран предмет: " + itemId);
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
	if (feltBreathing) out.messages.push_back("Вы чувствуете чьё-то дыхание поблизости.");
	consume_action_or_advance(*this, map);
	return out;
}

AttackOutcome Game::attack(const std::string& name, Direction dir, LabyrinthMap& map) {
	// Backward compatibility: attack = use knife
	AttackOutcome out;
	if (!is_players_turn(*this, name)) {
		out.messages.push_back("Сейчас не ваш ход");
		return out;
	}
	auto use = use_item(name, std::string("knife"), dir, map);
	out.attacked = use.used;
	out.messages = std::move(use.messages);
	return out;
}

UseOutcome Game::use_item(const std::string& name, const std::string& itemId, Direction dir, LabyrinthMap& map) {
	UseOutcome out;
	auto itp = players.find(name);
	if (itp == players.end()) { out.messages.push_back("Нет такого игрока"); return out; }
	if (!is_players_turn(*this, name)) {
		out.messages.push_back("Сейчас не ваш ход");
		return out;
	}
	// Create requested item
	std::unique_ptr<Item> item;
	if (itemId == "knife") item = std::make_unique<Knife>();
	else if (itemId == "shotgun") item = std::make_unique<Shotgun>();
	else if (itemId == "rifle") item = std::make_unique<Rifle>();
	else if (itemId == "flashlight") item = std::make_unique<Flashlight>();
	else { out.messages.push_back("Неизвестный предмет"); return out; }
	// Delegate charge logic to item
	extern bool item_use(Game& game, Item& item, LabyrinthMap& map, const std::string& playerName, Direction dir, std::vector<std::string>& messages);
	out.used = item_use(*this, *item, map, name, dir, out.messages);
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
	if (players_with_treasure.count(victim)) {
		players_with_treasure.erase(victim);
		loot_treasure[key_xy(itp->second.first, itp->second.second)] += 1;
	}
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

// Бот: убийство только в начале хода, если уже рядом с игроком не в больнице; иначе движение к видимому
// игроку или случайный шаг. В фид — только «Бот походил» (+ PLAYER при убийстве).
void Game::run_bot_turn(LabyrinthMap& map, std::vector<std::string>& messages, std::vector<BotReplayStep>* replay_log) {
	if (!bot_enabled) { advance_turn(*this, map); return; }
	if (players.empty()) { advance_turn(*this, map); return; }
	if (bot_x >= map.width || bot_y >= map.height) { advance_turn(*this, map); return; }

	auto try_kill_victim = [&](const std::string& victim) -> bool {
		auto itp = players.find(victim);
		if (itp == players.end()) return false;
		if (players_with_treasure.count(victim)) {
			players_with_treasure.erase(victim);
			loot_treasure[key_xy(itp->second.first, itp->second.second)] += 1;
		}
		if (auto* loc = getLocationFor(CellContent::Hospital)) {
			if (auto* hosp = dynamic_cast<HospitalLocation*>(loc)) {
				if (hosp->teleportToHospital(*this, map, victim)) {
					messages.push_back(std::string("PLAYER:") + victim + ": Вас убил бот. Вы в больнице.");
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

	// 1) Только в начале хода: сосед или та же клетка, игрок не в больнице (бот «не видит» больницу)
	std::vector<std::string> adj_start;
	for (const auto& kv : players) {
		if (player_stands_on_hospital(*this, map, kv.first)) continue;
		auto [px, py] = kv.second;
		size_t dx = (px > bot_x) ? (px - bot_x) : (bot_x - px);
		size_t dy = (py > bot_y) ? (py - bot_y) : (bot_y - py);
		if (dx + dy <= 1) adj_start.push_back(kv.first);
	}
	bool killed = false;
	if (!adj_start.empty() && (rand_u32() % 2) == 0) {
		std::string victim = adj_start[static_cast<size_t>(rand_u32() % adj_start.size())];
		killed = try_kill_victim(victim);
	}

	if (!killed) {
		// Есть ли игрок не на клетке больницы — потенциальная видимая цель для BFS?
		bool any_outside_hospital = false;
		for (const auto& kv : players) {
			if (!player_stands_on_hospital(*this, map, kv.first)) {
				any_outside_hospital = true;
				break;
			}
		}
		bool did_bfs_move = false;
		if (any_outside_hospital) {
			std::vector<std::vector<bool>> vis(map.height, std::vector<bool>(map.width, false));
			std::vector<std::vector<std::pair<int, int>>> prev(map.height, std::vector<std::pair<int, int>>(map.width, {-1, -1}));
			std::queue<std::pair<size_t, size_t>> q;
			q.push({bot_x, bot_y});
			vis[bot_y][bot_x] = true;
			std::pair<size_t, size_t> goal{map.width, map.height};
			auto is_visible_target = [&](size_t x, size_t y) -> bool {
				CellContent c = map.get_cell(x, y);
				if (c == CellContent::Hospital || c == CellContent::Arsenal) return false;
				for (const auto& kv : players) {
					if (kv.second.first == x && kv.second.second == y) return true;
				}
				return false;
			};
			while (!q.empty()) {
				auto [cx, cy] = q.front();
				q.pop();
				if (is_visible_target(cx, cy)) {
					goal = {cx, cy};
					break;
				}
				if (cx > 0 && map.can_move_left(cx, cy) && !vis[cy][cx - 1]) {
					vis[cy][cx - 1] = true;
					prev[cy][cx - 1] = {static_cast<int>(cx), static_cast<int>(cy)};
					q.push({cx - 1, cy});
				}
				if (cx + 1 < map.width && map.can_move_right(cx, cy) && !vis[cy][cx + 1]) {
					vis[cy][cx + 1] = true;
					prev[cy][cx + 1] = {static_cast<int>(cx), static_cast<int>(cy)};
					q.push({cx + 1, cy});
				}
				if (cy > 0 && map.can_move_up(cx, cy) && !vis[cy - 1][cx]) {
					vis[cy - 1][cx] = true;
					prev[cy - 1][cx] = {static_cast<int>(cx), static_cast<int>(cy)};
					q.push({cx, cy - 1});
				}
				if (cy + 1 < map.height && map.can_move_down(cx, cy) && !vis[cy + 1][cx]) {
					vis[cy + 1][cx] = true;
					prev[cy + 1][cx] = {static_cast<int>(cx), static_cast<int>(cy)};
					q.push({cx, cy + 1});
				}
			}
			std::vector<std::pair<size_t, size_t>> path;
			if (goal.first < map.width) {
				auto cur = goal;
				while (!(cur.first == bot_x && cur.second == bot_y)) {
					path.push_back(cur);
					auto p = prev[cur.second][cur.first];
					if (p.first < 0) break;
					cur = {static_cast<size_t>(p.first), static_cast<size_t>(p.second)};
				}
				std::reverse(path.begin(), path.end());
			}
			size_t steps = 0;
			while (steps < static_cast<size_t>(std::max(1, bot_steps_per_turn)) && !path.empty()) {
				auto [nx, ny] = path.front();
				path.erase(path.begin());
				bot_x = nx;
				bot_y = ny;
				did_bfs_move = true;
				if (replay_log) {
					BotReplayStep s;
					s.kind = BotReplayStep::Kind::Move;
					s.x = bot_x;
					s.y = bot_y;
					replay_log->push_back(s);
				}
				steps++;
			}
		}
		// Нет хода по BFS: все в больнице / цель не видна / цель недостижима — блуждание
		if (!did_bfs_move) {
			for (size_t s = 0; s < static_cast<size_t>(std::max(1, bot_steps_per_turn)); ++s) {
				if (!try_random_bot_step(*this, map, replay_log)) break;
			}
		}
	}

	messages.push_back("Бот походил");
	actions_left = 0;
	advance_turn(*this, map);
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
			messages.push_back("Игрок " + victim + " защищён бронёй! Броня уничтожена.");
			return false;
		}
	}

	auto pos = itp->second;
	if (game.players_with_treasure.count(victim)) {
		game.players_with_treasure.erase(victim);
		game.loot_treasure[key_xy(pos.first, pos.second)] += 1;
	}

	bool sent = false;
	if (auto* loc = getLocationFor(CellContent::Hospital)) {
		if (auto* hosp = dynamic_cast<HospitalLocation*>(loc)) {
			sent = hosp->teleportToHospital(game, map, victim);
		}
	}
	if (!sent) messages.push_back("Больница не найдена");
	return sent;
}
