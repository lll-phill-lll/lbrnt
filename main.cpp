#include "generator.hpp"
#include "state.hpp"
#include "viz.hpp"
#include "items/Knife.hpp"
#include "items/Shotgun.hpp"
#include "items/Rifle.hpp"
#include "items/Flashlight.hpp"
#include "items/Armor.hpp"
#include <iostream>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <memory>

static std::unique_ptr<Item> makeItem(const std::string& id) {
	if (id == "knife") return std::make_unique<Knife>();
	if (id == "shotgun") return std::make_unique<Shotgun>();
	if (id == "rifle") return std::make_unique<Rifle>();
	if (id == "flashlight") return std::make_unique<Flashlight>();
	if (id == "armor") return std::make_unique<Armor>();
	return nullptr;
}

static std::string jsonEscape(const std::string& s) {
	std::string out;
	for (char c : s) {
		if (c == '"') out += "\\\"";
		else if (c == '\\') out += "\\\\";
		else if (c == '\n') out += "\\n";
		else out += c;
	}
	return out;
}

// Simple stderr logger for service messages
static void log_err(const std::string& msg) {
	std::cerr << msg << "\n";
}

static void applyLogEntry(const LogEntry& e, AppState& cur) {
	bool wasEnforced = cur.game.enforce_turns;
	cur.game.enforce_turns = false;
	switch (e.type) {
		case LogType::AddPlayer:
		case LogType::AddPlayerRandom: {
			std::string eerr;
			cur.game.add_player(e.name, {e.x, e.y}, cur.map, eerr);
			break;
		}
		case LogType::Move:
			cur.game.move_player(e.name, e.dir, cur.map);
			break;
		case LogType::Attack:
			cur.game.attack(e.name, e.dir, cur.map);
			break;
		case LogType::UseItem: {
			std::unique_ptr<Item> itm;
			if (e.item == "knife") itm = std::make_unique<Knife>();
			else if (e.item == "shotgun") itm = std::make_unique<Shotgun>();
			else if (e.item == "rifle") itm = std::make_unique<Rifle>();
			else if (e.item == "flashlight") itm = std::make_unique<Flashlight>();
			if (itm) {
				std::vector<std::string> msgs;
				itm->apply(cur.game, cur.map, e.name, e.dir, msgs);
			}
			break;
		}
		case LogType::BotMove:
			cur.game.bot_x = e.x;
			cur.game.bot_y = e.y;
			break;
		case LogType::BotKill:
			cur.game.apply_replay_bot_kill(e.name, cur.map);
			break;
	}
	cur.game.enforce_turns = wasEnforced;
}

static std::string logEntryDescription(const LogEntry& e) {
	auto dirStr = [](Direction d) -> std::string {
		switch(d) {
			case Direction::Up: return "вверх";
			case Direction::Down: return "вниз";
			case Direction::Left: return "влево";
			case Direction::Right: return "вправо";
		}
		return "";
	};
	switch (e.type) {
		case LogType::AddPlayer: return e.name + " добавлен на (" + std::to_string(e.x) + "," + std::to_string(e.y) + ")";
		case LogType::AddPlayerRandom: return e.name + " добавлен (случайно)";
		case LogType::Move: return e.name + " → " + dirStr(e.dir);
		case LogType::Attack: return e.name + " атакует " + dirStr(e.dir);
		case LogType::UseItem: return e.name + " использует " + e.item + " " + dirStr(e.dir);
		case LogType::BotMove: return std::string("бот → (") + std::to_string(e.x) + "," + std::to_string(e.y) + ")";
		case LogType::BotKill: return std::string("бот убил ") + e.name;
	}
	return "";
}

// User-facing formatted output: [Player]: then tab-indented messages
static void print_user_messages(const std::string& player, const std::vector<std::string>& messages) {
	std::cout << "[" << player << "]:" << "\n";
	for (const auto& m : messages) {
		std::cout << "\t" << m << "\n";
	}
}

// Run all consecutive bot turns (same loop as after move/attack/use-item).
// Needed when the web server has no player action to trigger the CLI (e.g. state file already says "bot" turn).
// Hard cap: if every human is in hospital, advance_turn can pick "bot" again forever — avoid hanging the process.
static void run_pending_bot_turns(AppState& st) {
	const int kMaxBotSteps = 512;
	for (int step = 0; step < kMaxBotSteps; ++step) {
		if (!st.game.enforce_turns || !st.game.bot_enabled || st.game.turn_order.empty()) break;
		if (st.game.turn_index >= st.game.turn_order.size()) break;
		if (st.game.turn_order[st.game.turn_index] != "bot") break;
		std::vector<std::string> blog;
		std::vector<BotReplayStep> bot_replay;
		st.game.run_bot_turn(st.map, blog, &bot_replay);
		for (const auto& s : bot_replay) {
			if (s.kind == BotReplayStep::Kind::Move)
				st.log.push_back(LogEntry{LogType::BotMove, "", Direction::Up, s.x, s.y, {}});
			else
				st.log.push_back(LogEntry{LogType::BotKill, s.victim, Direction::Up, 0, 0, {}});
		}
		for (const auto& line : blog) {
			if (line.rfind("PLAYER:", 0) == 0) {
				size_t p = line.find(':', 7);
				if (p != std::string::npos) {
					std::string pname = line.substr(7, p - 7);
					std::string pmsg = line.substr(p + 1);
					print_user_messages(pname, std::vector<std::string>{pmsg});
				}
			}
		}
		// В фиде одна строка на ход бота (без пошаговых координат)
		for (const auto& line : blog) {
			if (line == "Бот походил") {
				std::cout << "Бот походил\n";
				break;
			}
		}
	}
	if (st.game.enforce_turns && st.game.bot_enabled && !st.game.turn_order.empty()
	    && st.game.turn_index < st.game.turn_order.size()
	    && st.game.turn_order[st.game.turn_index] == "bot") {
		log_err("run_pending_bot_turns: iteration cap reached (stuck on bot turn; check hospital/turn logic)");
	}
}

static void usage() {
	std::cout <<
R"(labyrinth_cpp commands:
  generate --width W --height H --out state.txt [--openness 0..1] [--seed N]
            [--turns 0|1]
            [--turn-actions N]
            [--bot-steps N]
  show --state state.txt [--reveal]
  status --state state.txt
  player-status --state state.txt --name NAME
  add-player --state state.txt --name NAME --x X --y Y
  add-player-random --state state.txt --name NAME
  move --state state.txt --name NAME (up|down|left|right)
  attack --state state.txt --name NAME (up|down|left|right)
  use-item --state state.txt --name NAME --item (knife|shotgun|rifle|flashlight) (up|down|left|right)
  set-cell --state state.txt --x X --y Y (empty|treasure|hospital|arsenal|exit)
  set-vwall --state state.txt --x X --y Y (--present=0|1)
  set-hwall --state state.txt --x X --y Y (--present=0|1)
  set-knife --state state.txt --name NAME (--broken=0|1)
  set-turns --state state.txt (0|1)
  add-item --state state.txt --item (knife|shotgun|rifle|flashlight|armor) --x X --y Y [--charges N]
  add-item-random --state state.txt --item (knife|shotgun|rifle|flashlight|armor) [--charges N]
  save-as --state state.txt --out other.txt
  export-svg --state state.txt --out maze.svg [--cell N] [--margin PX]
  export-html --state state.txt --out maze.html [--cell N] [--margin PX]
  replay-export --base base.txt --log state_with_log.txt --out-dir frames --cell N --margin PX
  replay-list --state state.txt
  replay-svg --state state.txt --step N
  init-turns --state state.txt
  init-base --state state.txt
  resolve-bots --state state.txt
  replay-export-one --state state.txt --out-dir frames --cell N --margin PX
)";
}

// Deterministic PRNG based on state RNG seed and nonce
static uint64_t splitmix64(uint64_t x) {
	x += 0x9e3779b97f4a7c15ull;
	x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
	x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
	return x ^ (x >> 31);
}
static size_t rng_pick(AppState& st, size_t maxExclusive) {
	uint64_t key = (static_cast<uint64_t>(st.random_seed) << 32) ^ st.random_nonce;
	uint64_t r = splitmix64(key);
	st.random_nonce += 1;
	return static_cast<size_t>(r % static_cast<uint64_t>(maxExclusive));
}

static bool get_flag(int argc, char** argv, const std::string& key) {
	for (int i = 1; i < argc; ++i) { if (std::string(argv[i]) == key) return true; }
	return false;
}
static bool get_arg(int argc, char** argv, const std::string& key, std::string& value) {
	for (int i = 1; i + 1 < argc; ++i) {
		if (std::string(argv[i]) == key) { value = argv[i+1]; return true; }
	}
	return false;
}

int main(int argc, char** argv) {
	if (argc < 2) { usage(); return 1; }
	std::string cmd = argv[1];
	if (cmd == "generate") {
		std::string sw, sh, out, so, sseed, sturns, sactions, sbot;
		if (!get_arg(argc, argv, std::string("--width"), sw) ||
		    !get_arg(argc, argv, std::string("--height"), sh) ||
		    !get_arg(argc, argv, std::string("--out"), out)) {
			usage(); return 1;
		}
		size_t w = static_cast<size_t>(std::stoul(sw));
		size_t h = static_cast<size_t>(std::stoul(sh));
		float openness = 0.0f;
		if (get_arg(argc, argv, std::string("--openness"), so)) {
			openness = std::stof(so);
			if (openness < 0.0f) openness = 0.0f;
			if (openness > 1.0f) openness = 1.0f;
		}
		unsigned int seed = std::random_device{}();
		if (get_arg(argc, argv, std::string("--seed"), sseed)) {
			seed = static_cast<unsigned int>(std::stoul(sseed));
		}
		set_rng_seed(seed);
		AppState st;
		st.random_seed = seed;
		st.random_nonce = 0;
		// turns enabled by default
		st.game.enforce_turns = true;
		if (get_arg(argc, argv, std::string("--turns"), sturns)) {
			int tv = std::stoi(sturns);
			st.game.enforce_turns = (tv != 0);
		}
		st.game.actions_per_turn = 1;
		if (get_arg(argc, argv, std::string("--turn-actions"), sactions)) {
			int av = std::stoi(sactions);
			if (av < 1) av = 1;
			st.game.actions_per_turn = av;
		}
		st.game.actions_left = st.game.actions_per_turn;
		st.map = generate_maze_with_items(w, h, openness);
		// Bot enable and initial placement
		st.game.bot_enabled = false;
		if (get_arg(argc, argv, std::string("--bot-steps"), sbot)) {
			int bs = std::stoi(sbot);
			if (bs > 0) {
				st.game.bot_enabled = true;
				st.game.bot_steps_per_turn = bs;
				// place bot to random empty cell
				std::vector<std::pair<size_t,size_t>> spots;
				for (size_t y = 0; y < st.map.height; ++y)
					for (size_t x = 0; x < st.map.width; ++x)
						if (st.map.get_cell(x,y) == CellContent::Empty) spots.emplace_back(x,y);
				if (!spots.empty()) {
					auto pos = spots[rng_pick(st, spots.size())];
					st.game.bot_x = pos.first; st.game.bot_y = pos.second;
				} else {
					st.game.bot_x = 0; st.game.bot_y = 0;
				}
			}
		}
		std::string err;
		if (!AppState::save(st, out, err)) { std::cerr << err << "\n"; return 2; }
		std::cout << "Создано: " << out << "\n";
		return 0;
	}
	if (cmd == "show") {
		std::string state;
		if (!get_arg(argc, argv, std::string("--state"), state)) { usage(); return 1; }
		bool reveal = get_flag(argc, argv, std::string("--reveal"));
		AppState st; std::string err;
		if (!AppState::load(st, state, err)) { std::cerr << err << "\n"; return 2; }
		std::cout << st.map.render_ascii(&st.game.players, reveal, &st.game.loot_treasure) << std::flush;
		return 0;
	}
	if (cmd == "status") {
		std::string state;
		if (!get_arg(argc, argv, std::string("--state"), state)) { usage(); return 1; }
		AppState st; std::string err;
		if (!AppState::load(st, state, err)) { std::cerr << err << "\n"; return 2; }
		// determine next actor
		std::string nextActor = "-";
		if (st.game.enforce_turns && !st.game.turn_order.empty()) {
			size_t idx = st.game.turn_index;
			// find first present from idx forward circularly
			for (size_t k = 0; k < st.game.turn_order.size(); ++k) {
				const std::string& cand = st.game.turn_order[(idx + k) % st.game.turn_order.size()];
				if (st.game.players.count(cand)) { nextActor = cand; break; }
			}
		}
		// print global turn info
		print_user_messages("TURN", std::vector<std::string>{std::string("Next: ") + nextActor});
		// build player listing order
		std::vector<std::string> names;
		if (st.game.enforce_turns && !st.game.turn_order.empty()) {
			for (const auto& n : st.game.turn_order) if (st.game.players.count(n)) names.push_back(n);
			for (const auto& kv : st.game.players) {
				if (std::find(names.begin(), names.end(), kv.first) == names.end()) names.push_back(kv.first);
			}
		} else {
			for (const auto& kv : st.game.players) names.push_back(kv.first);
			std::sort(names.begin(), names.end());
		}
		// per-player inventories
		for (const auto& name : names) {
			std::vector<std::string> lines;
			auto itInv = st.game.inventories.find(name);
			if (itInv == st.game.inventories.end() || itInv->second.item_charges.empty()) {
				lines.push_back("Inventory: (empty)");
			} else {
				// stable item order
				static const char* order[] = {"knife","rifle","shotgun","flashlight","armor"};
				for (const char* iid : order) {
					auto it = itInv->second.item_charges.find(iid);
					if (it == itInv->second.item_charges.end()) continue;
					int c = it->second;
					std::string label = std::string(iid) + ": " + std::to_string(c);
					if (std::string(iid) == "knife" && st.game.broken_knife.count(name)) label += " [broken]";
					lines.push_back(label);
				}
				// any extra items not in predefined order
				for (const auto& kv : itInv->second.item_charges) {
					if (kv.first=="knife" || kv.first=="rifle" || kv.first=="shotgun" || kv.first=="flashlight" || kv.first=="armor") continue;
					lines.push_back(kv.first + ": " + std::to_string(kv.second));
				}
				if (lines.empty()) lines.push_back("Inventory: (empty)");
			}
			print_user_messages(name, lines);
		}
		return 0;
	}
	if (cmd == "player-status") {
		std::string state, name;
		if (!get_arg(argc, argv, std::string("--state"), state) ||
		    !get_arg(argc, argv, std::string("--name"), name)) { usage(); return 1; }
		AppState st; std::string err;
		if (!AppState::load(st, state, err)) { std::cerr << err << "\n"; return 2; }
		if (!st.game.players.count(name)) { std::cerr << "Игрок не найден\n"; return 3; }

		bool hasTreasure = st.game.players_with_treasure.count(name) > 0;

		std::ostringstream js;
		js << "{";
		js << "\"name\":\"" << jsonEscape(name) << "\",";
		js << "\"hasTreasure\":" << (hasTreasure ? "true" : "false") << ",";
		js << "\"items\":[";

		static const char* itemOrder[] = {"knife","shotgun","rifle","flashlight","armor"};
		auto itInv = st.game.inventories.find(name);
		bool first = true;
		for (const char* iid : itemOrder) {
			int charges = 0;
			bool has = false;
			if (itInv != st.game.inventories.end()) {
				auto it = itInv->second.item_charges.find(iid);
				if (it != itInv->second.item_charges.end()) {
					charges = it->second;
					has = true;
				}
			}
			if (!has) continue;
			auto item = makeItem(iid);
			if (!item) continue;

			if (!first) js << ",";
			first = false;
			bool broken = (std::string(iid) == "knife" && st.game.broken_knife.count(name));
			js << "{";
			js << "\"id\":\"" << iid << "\",";
			js << "\"displayName\":\"" << jsonEscape(item->displayName()) << "\",";
			js << "\"description\":\"" << jsonEscape(item->description()) << "\",";
			js << "\"rechargeHint\":\"" << jsonEscape(item->rechargeHint()) << "\",";
			js << "\"charges\":" << charges << ",";
			js << "\"broken\":" << (broken ? "true" : "false");
			js << "}";
		}
		// extra items not in predefined order
		if (itInv != st.game.inventories.end()) {
			for (const auto& kv : itInv->second.item_charges) {
				if (kv.first=="knife"||kv.first=="shotgun"||kv.first=="rifle"||kv.first=="flashlight"||kv.first=="armor") continue;
				auto item = makeItem(kv.first);
				if (!first) js << ",";
				first = false;
				js << "{\"id\":\"" << jsonEscape(kv.first) << "\",";
				js << "\"displayName\":\"" << jsonEscape(kv.first) << "\",";
				js << "\"description\":\"\",\"rechargeHint\":\"\",";
				js << "\"charges\":" << kv.second << ",\"broken\":false}";
			}
		}

		js << "],";

		// Breathing detection
		auto myPos = st.game.players[name];
		bool breathing = false;
		bool meInHospital = st.map.get_cell(myPos.first, myPos.second) == CellContent::Hospital;
		for (const auto& kv : st.game.players) {
			if (kv.first == name) continue;
			if (st.map.get_cell(kv.second.first, kv.second.second) == CellContent::Hospital) continue;
			size_t dx = (myPos.first > kv.second.first) ? (myPos.first - kv.second.first) : (kv.second.first - myPos.first);
			size_t dy = (myPos.second > kv.second.second) ? (myPos.second - kv.second.second) : (kv.second.second - myPos.second);
			if (dx + dy != 1) continue;
			bool vis = false;
			if (kv.second.first + 1 == myPos.first && kv.second.second == myPos.second) vis = st.map.can_move_left(myPos.first, myPos.second);
			else if (kv.second.first == myPos.first + 1 && kv.second.second == myPos.second) vis = st.map.can_move_right(myPos.first, myPos.second);
			else if (kv.second.second + 1 == myPos.second && kv.second.first == myPos.first) vis = st.map.can_move_up(myPos.first, myPos.second);
			else if (kv.second.second == myPos.second + 1 && kv.second.first == myPos.first) vis = st.map.can_move_down(myPos.first, myPos.second);
			if (vis) { breathing = true; break; }
		}
		if (meInHospital) breathing = false;
		if (!breathing && !meInHospital && st.game.bot_enabled) {
			size_t bx = st.game.bot_x, by = st.game.bot_y;
			size_t dx = (myPos.first > bx) ? (myPos.first - bx) : (bx - myPos.first);
			size_t dy = (myPos.second > by) ? (myPos.second - by) : (by - myPos.second);
			if (dx + dy == 1) {
				if (bx + 1 == myPos.first && by == myPos.second) breathing = st.map.can_move_left(myPos.first, myPos.second);
				else if (bx == myPos.first + 1 && by == myPos.second) breathing = st.map.can_move_right(myPos.first, myPos.second);
				else if (by + 1 == myPos.second && bx == myPos.first) breathing = st.map.can_move_up(myPos.first, myPos.second);
				else if (by == myPos.second + 1 && bx == myPos.first) breathing = st.map.can_move_down(myPos.first, myPos.second);
			}
		}
		js << "\"nearbyBreathing\":" << (breathing ? "true" : "false");
		js << "}";
		std::cout << js.str() << "\n";
		return 0;
	}
	if (cmd == "add-player") {
		std::string state, name, sx, sy;
		if (!get_arg(argc, argv, std::string("--state"), state) ||
		    !get_arg(argc, argv, std::string("--name"), name) ||
		    !get_arg(argc, argv, std::string("--x"), sx) ||
		    !get_arg(argc, argv, std::string("--y"), sy)) { usage(); return 1; }
		AppState st; std::string err;
		if (!AppState::load(st, state, err)) { std::cerr << err << "\n"; return 2; }
		std::string e;
		if (!st.game.add_player(name, {static_cast<size_t>(std::stoul(sx)), static_cast<size_t>(std::stoul(sy))}, st.map, e)) {
			std::cerr << e << "\n"; return 3;
		}
		if (!AppState::save(st, state, err)) { std::cerr << err << "\n"; return 2; }
		log_err(std::string("Игрок '") + name + "' добавлен");
		// log
		st.log.push_back(LogEntry{LogType::AddPlayer, name, Direction::Up, (size_t)std::stoul(sx), (size_t)std::stoul(sy), {}});
		return 0; // no stdout response
	}
	if (cmd == "add-player-random") {
		std::string state, name;
		if (!get_arg(argc, argv, std::string("--state"), state) ||
		    !get_arg(argc, argv, std::string("--name"), name)) { usage(); return 1; }
		AppState st; std::string err;
		if (!AppState::load(st, state, err)) { std::cerr << err << "\n"; return 2; }
		// collect empty, unoccupied cells
		std::unordered_set<long long> occupied;
		for (const auto& kv : st.game.players) {
			occupied.insert((long long)kv.second.second * 1000000LL + (long long)kv.second.first);
		}
		std::vector<std::pair<size_t,size_t>> spots;
		for (size_t y = 0; y < st.map.height; ++y) {
			for (size_t x = 0; x < st.map.width; ++x) {
				if (st.map.get_cell(x, y) == CellContent::Empty) {
					long long key = (long long)y * 1000000LL + (long long)x;
					if (!occupied.count(key)) spots.emplace_back(x, y);
				}
			}
		}
		if (spots.empty()) { std::cerr << "Нет свободных клеток для размещения\n"; return 3; }
		// С ботом: не ставить игрока на соседнюю с ботом клетку (манхэттен ≤ 1), иначе при первом же
		// resolve-bots / ходе бота он может убить сразу — кажется, что «всегда спавн в больнице».
		if (st.game.bot_enabled) {
			std::vector<std::pair<size_t,size_t>> far_from_bot;
			far_from_bot.reserve(spots.size());
			for (auto [x, y] : spots) {
				size_t dx = (x > st.game.bot_x) ? (x - st.game.bot_x) : (st.game.bot_x - x);
				size_t dy = (y > st.game.bot_y) ? (y - st.game.bot_y) : (st.game.bot_y - y);
				if (dx + dy >= 2) far_from_bot.emplace_back(x, y);
			}
			if (!far_from_bot.empty()) spots = std::move(far_from_bot);
		}
		auto pos = spots[rng_pick(st, spots.size())];
		std::string e;
		if (!st.game.add_player(name, pos, st.map, e)) { std::cerr << e << "\n"; return 3; }
		st.log.push_back(LogEntry{LogType::AddPlayerRandom, name, Direction::Up, pos.first, pos.second, {}});
		if (!AppState::save(st, state, err)) { std::cerr << err << "\n"; return 2; }
		log_err(std::string("Игрок '") + name + "' добавлен на " + std::to_string(pos.first) + "," + std::to_string(pos.second));
		return 0; // no stdout response
	}
	if (cmd == "move") {
		std::string state, name, sdir;
		if (!get_arg(argc, argv, std::string("--state"), state) ||
		    !get_arg(argc, argv, std::string("--name"), name)) { usage(); return 1; }
		if (argc < 3) { usage(); return 1; }
		sdir = argv[argc-1];
		Direction dir;
		if      (sdir == "up") dir = Direction::Up;
		else if (sdir == "down") dir = Direction::Down;
		else if (sdir == "left") dir = Direction::Left;
		else if (sdir == "right") dir = Direction::Right;
		else { usage(); return 1; }
		AppState st; std::string err;
		if (!AppState::load(st, state, err)) { std::cerr << err << "\n"; return 2; }
		// detailed stderr log baseline
		std::pair<size_t,size_t> oldpos{0,0};
		bool had_old = false;
		{
			auto itp = st.game.players.find(name);
			if (itp != st.game.players.end()) { oldpos = itp->second; had_old = true; }
		}
		auto out = st.game.move_player(name, dir, st.map);
		st.log.push_back(LogEntry{LogType::Move, name, dir, 0, 0, {}});
		if (!AppState::save(st, state, err)) { std::cerr << err << "\n"; return 2; }
		// stderr detailed log
		if (had_old) {
			std::ostringstream es;
			es << "MOVE " << name << " from " << oldpos.first << "," << oldpos.second
			   << " to " << out.position.first << "," << out.position.second
			   << (out.moved ? " [moved]" : " [blocked]");
			log_err(es.str());
		} else {
			log_err(std::string("MOVE ") + name + " (no previous position)");
		}
		// stdout user messages
		print_user_messages(name, out.messages);
		run_pending_bot_turns(st);
		if (!AppState::save(st, state, err)) { std::cerr << err << "\n"; return 2; }
		return 0;
	}
	if (cmd == "use-item") {
		std::string state, name, item, sdir;
		if (!get_arg(argc, argv, std::string("--state"), state) ||
		    !get_arg(argc, argv, std::string("--name"), name) ||
		    !get_arg(argc, argv, std::string("--item"), item)) { usage(); return 1; }
		if (argc < 3) { usage(); return 1; }
		sdir = argv[argc-1];
		Direction dir;
		if      (sdir == "up") dir = Direction::Up;
		else if (sdir == "down") dir = Direction::Down;
		else if (sdir == "left") dir = Direction::Left;
		else if (sdir == "right") dir = Direction::Right;
		else { usage(); return 1; }
		AppState st; std::string err;
		if (!AppState::load(st, state, err)) { std::cerr << err << "\n"; return 2; }
		auto out = st.game.use_item(name, item, dir, st.map);
		st.log.push_back(LogEntry{LogType::UseItem, name, dir, 0, 0, item});
		if (!AppState::save(st, state, err)) { std::cerr << err << "\n"; return 2; }
		{
			std::ostringstream es;
			es << "USE " << name << " item=" << item << " dir=" << sdir << (out.used ? " [applied]" : " [failed]");
			log_err(es.str());
		}
		// stdout user messages
		print_user_messages(name, out.messages);
		run_pending_bot_turns(st);
		if (!AppState::save(st, state, err)) { std::cerr << err << "\n"; return 2; }
		return 0;
	}
	if (cmd == "add-item") {
		std::string state, item, sx, sy, sch;
		if (!get_arg(argc, argv, std::string("--state"), state) ||
		    !get_arg(argc, argv, std::string("--item"), item) ||
		    !get_arg(argc, argv, std::string("--x"), sx) ||
		    !get_arg(argc, argv, std::string("--y"), sy)) { usage(); return 1; }
		int charges = 1;
		if (get_arg(argc, argv, std::string("--charges"), sch)) charges = std::stoi(sch);
		if (item!="knife" && item!="shotgun" && item!="rifle" && item!="flashlight" && item!="armor") { usage(); return 1; }
		AppState st; std::string err;
		if (!AppState::load(st, state, err)) { std::cerr << err << "\n"; return 2; }
		size_t x = static_cast<size_t>(std::stoul(sx));
		size_t y = static_cast<size_t>(std::stoul(sy));
		if (!st.map.in_bounds((long)x,(long)y)) { std::cerr << "Вне карты\n"; return 3; }
		if (st.map.get_cell(x,y) != CellContent::Empty) { std::cerr << "Клетка занята не-пустой меткой\n"; return 3; }
		long long key = (long long)y * 1000000LL + (long long)x;
		st.game.ground_items[key][item] += std::max(1, charges);
		if (!AppState::save(st, state, err)) { std::cerr << err << "\n"; return 2; }
		std::cout << "OK\n"; return 0;
	}
	if (cmd == "add-item-random") {
		std::string state, item, sch;
		if (!get_arg(argc, argv, std::string("--state"), state) ||
		    !get_arg(argc, argv, std::string("--item"), item)) { usage(); return 1; }
		int charges = 1;
		if (get_arg(argc, argv, std::string("--charges"), sch)) charges = std::stoi(sch);
		if (item!="knife" && item!="shotgun" && item!="rifle" && item!="flashlight" && item!="armor") { usage(); return 1; }
		AppState st; std::string err;
		if (!AppState::load(st, state, err)) { std::cerr << err << "\n"; return 2; }
		// collect empty cells without existing ground items or special content
		std::vector<std::pair<size_t,size_t>> spots;
		for (size_t y = 0; y < st.map.height; ++y) {
			for (size_t x = 0; x < st.map.width; ++x) {
				if (st.map.get_cell(x, y) != CellContent::Empty) continue;
				long long key = (long long)y * 1000000LL + (long long)x;
				// allow multiple items per cell; no filter
				spots.emplace_back(x, y);
			}
		}
		if (spots.empty()) { std::cerr << "Нет пустых клеток для размещения\n"; return 3; }
		auto pos = spots[rng_pick(st, spots.size())];
		long long key = (long long)pos.second * 1000000LL + (long long)pos.first;
		st.game.ground_items[key][item] += std::max(1, charges);
		if (!AppState::save(st, state, err)) { std::cerr << err << "\n"; return 2; }
		std::cout << "Предмет '" << item << "' добавлен на " << pos.first << "," << pos.second << "\n";
		return 0;
	}
	if (cmd == "attack") {
		std::string state, name, sdir;
		if (!get_arg(argc, argv, std::string("--state"), state) ||
		    !get_arg(argc, argv, std::string("--name"), name)) { usage(); return 1; }
		if (argc < 3) { usage(); return 1; }
		sdir = argv[argc-1];
		Direction dir;
		if      (sdir == "up") dir = Direction::Up;
		else if (sdir == "down") dir = Direction::Down;
		else if (sdir == "left") dir = Direction::Left;
		else if (sdir == "right") dir = Direction::Right;
		else { usage(); return 1; }
		AppState st; std::string err;
		if (!AppState::load(st, state, err)) { std::cerr << err << "\n"; return 2; }
		auto out = st.game.attack(name, dir, st.map);
		st.log.push_back(LogEntry{LogType::Attack, name, dir, 0, 0, {}});
		if (!AppState::save(st, state, err)) { std::cerr << err << "\n"; return 2; }
		// stderr detailed log
		{
			std::ostringstream es;
			es << "ATTACK " << name << " dir=" << sdir << (out.attacked ? " [done]" : " [failed]");
			log_err(es.str());
		}
		// stdout user messages
		print_user_messages(name, out.messages);
		run_pending_bot_turns(st);
		if (!AppState::save(st, state, err)) { std::cerr << err << "\n"; return 2; }
		return 0;
	}
	if (cmd == "replay-export") {
		std::string base, logfile, outdir, scell, smargin;
		if (!get_arg(argc, argv, std::string("--base"), base) ||
		    !get_arg(argc, argv, std::string("--log"), logfile) ||
		    !get_arg(argc, argv, std::string("--out-dir"), outdir)) { usage(); return 1; }
		float cell = 36.0f, margin = 24.0f;
		if (get_arg(argc, argv, std::string("--cell"), scell)) cell = std::stof(scell);
		if (get_arg(argc, argv, std::string("--margin"), smargin)) margin = std::stof(smargin);
		AppState st0, stlog; std::string err;
		if (!AppState::load(st0, base, err)) { std::cerr << "Base: " << err << "\n"; return 2; }
		if (!AppState::load(stlog, logfile, err)) { std::cerr << "Log: " << err << "\n"; return 2; }
		// ensure outdir exists
		std::filesystem::create_directories(outdir);
		// work copy
		AppState cur = st0;
		// frame 0
		{
			std::string svg = render_svg(cur, cell, margin);
			std::ofstream f(outdir + "/frame_0000.svg"); f << svg;
		}
		int step = 1;
		for (const auto& e : stlog.log) {
			applyLogEntry(e, cur);
			char buf[64];
			std::snprintf(buf, sizeof(buf), "/frame_%04d.svg", step++);
			std::string svg = render_svg(cur, cell, margin);
			std::ofstream f(outdir + buf); f << svg;
		}
		std::cout << "Exported " << step << " frames to " << outdir << "\n";
		return 0;
	}
	if (cmd == "set-cell") {
		std::string state, sx, sy, sval;
		if (!get_arg(argc, argv, std::string("--state"), state) ||
		    !get_arg(argc, argv, std::string("--x"), sx) ||
		    !get_arg(argc, argv, std::string("--y"), sy)) { usage(); return 1; }
		if (argc < 3) { usage(); return 1; }
		sval = argv[argc-1];
		CellContent c = CellContent::Empty;
		if      (sval == "empty") c = CellContent::Empty;
		else if (sval == "treasure") c = CellContent::Treasure;
		else if (sval == "hospital") c = CellContent::Hospital;
		else if (sval == "arsenal") c = CellContent::Arsenal;
		else if (sval == "exit") c = CellContent::Exit;
		else { usage(); return 1; }
		AppState st; std::string err;
		if (!AppState::load(st, state, err)) { std::cerr << err << "\n"; return 2; }
		st.map.set_cell(static_cast<size_t>(std::stoul(sx)), static_cast<size_t>(std::stoul(sy)), c);
		if (!AppState::save(st, state, err)) { std::cerr << err << "\n"; return 2; }
		std::cout << "OK\n"; return 0;
	}
	if (cmd == "set-vwall") {
		std::string state, sx, sy, sp;
		if (!get_arg(argc, argv, std::string("--state"), state) ||
		    !get_arg(argc, argv, std::string("--x"), sx) ||
		    !get_arg(argc, argv, std::string("--y"), sy) ||
		    !get_arg(argc, argv, std::string("--present"), sp)) { usage(); return 1; }
		AppState st; std::string err;
		if (!AppState::load(st, state, err)) { std::cerr << err << "\n"; return 2; }
		st.map.set_vwall(static_cast<size_t>(std::stoul(sy)), static_cast<size_t>(std::stoul(sx)), sp != "0");
		if (!AppState::save(st, state, err)) { std::cerr << err << "\n"; return 2; }
		std::cout << "OK\n"; return 0;
	}
	if (cmd == "set-hwall") {
		std::string state, sx, sy, sp;
		if (!get_arg(argc, argv, std::string("--state"), state) ||
		    !get_arg(argc, argv, std::string("--x"), sx) ||
		    !get_arg(argc, argv, std::string("--y"), sy) ||
		    !get_arg(argc, argv, std::string("--present"), sp)) { usage(); return 1; }
		AppState st; std::string err;
		if (!AppState::load(st, state, err)) { std::cerr << err << "\n"; return 2; }
		st.map.set_hwall(static_cast<size_t>(std::stoul(sy)), static_cast<size_t>(std::stoul(sx)), sp != "0");
		if (!AppState::save(st, state, err)) { std::cerr << err << "\n"; return 2; }
		std::cout << "OK\n"; return 0;
	}
	if (cmd == "save-as") {
		std::string state, out;
		if (!get_arg(argc, argv, std::string("--state"), state) ||
		    !get_arg(argc, argv, std::string("--out"), out)) { usage(); return 1; }
		AppState st; std::string err;
		if (!AppState::load(st, state, err)) { std::cerr << err << "\n"; return 2; }
		if (!AppState::save(st, out, err)) { std::cerr << err << "\n"; return 2; }
		std::cout << "Сохранено как: " << out << "\n"; return 0;
	}
	if (cmd == "export-svg") {
		std::string state, out, scell, smargin;
		if (!get_arg(argc, argv, std::string("--state"), state) ||
		    !get_arg(argc, argv, std::string("--out"), out)) { usage(); return 1; }
		float cell = 32.0f;
		if (get_arg(argc, argv, std::string("--cell"), scell)) cell = std::stof(scell);
		float margin = cell * 0.5f;
		if (get_arg(argc, argv, std::string("--margin"), smargin)) margin = std::stof(smargin);
		AppState st; std::string err;
		if (!AppState::load(st, state, err)) { std::cerr << err << "\n"; return 2; }
		auto svg = render_svg(st, cell, margin);
		std::ofstream f(out);
		if (!f) { std::cerr << "Не могу записать SVG\n"; return 2; }
		f << svg;
		log_err(std::string("SVG сохранён: ") + out);
		return 0; // no stdout response
	}
	if (cmd == "export-html") {
		std::string state, out, scell, smargin;
		if (!get_arg(argc, argv, std::string("--state"), state) ||
		    !get_arg(argc, argv, std::string("--out"), out)) { usage(); return 1; }
		float cell = 32.0f;
		if (get_arg(argc, argv, std::string("--cell"), scell)) cell = std::stof(scell);
		float margin = cell * 0.5f;
		if (get_arg(argc, argv, std::string("--margin"), smargin)) margin = std::stof(smargin);
		AppState st; std::string err;
		if (!AppState::load(st, state, err)) { std::cerr << err << "\n"; return 2; }
		auto html = render_html(st, cell, margin);
		std::ofstream f(out);
		if (!f) { std::cerr << "Не могу записать HTML\n"; return 2; }
		f << html;
		log_err(std::string("HTML сохранён: ") + out);
		return 0;
	}
	if (cmd == "init-turns") {
		std::string state;
		if (!get_arg(argc, argv, std::string("--state"), state)) { usage(); return 1; }
		AppState st; std::string err;
		if (!AppState::load(st, state, err)) { std::cerr << err << "\n"; return 2; }
		st.game.init_turns();
		if (!AppState::save(st, state, err)) { std::cerr << err << "\n"; return 2; }
		// Output turn info so callers can read it
		if (!st.game.turn_order.empty()) {
			std::string cur = st.game.turn_order[st.game.turn_index % st.game.turn_order.size()];
			std::cout << "Current: " << cur << "\n";
			std::cout << "Order:";
			for (const auto& n : st.game.turn_order) std::cout << " " << n;
			std::cout << "\n";
		}
		return 0;
	}
	if (cmd == "replay-list") {
		std::string state;
		if (!get_arg(argc, argv, std::string("--state"), state)) { usage(); return 1; }
		AppState st; std::string err;
		if (!AppState::load(st, state, err)) { std::cerr << err << "\n"; return 2; }
		std::ostringstream js;
		js << "{\"total\":" << st.log.size() << ",\"entries\":[";
		for (size_t i = 0; i < st.log.size(); ++i) {
			if (i) js << ",";
			js << "\"" << jsonEscape(logEntryDescription(st.log[i])) << "\"";
		}
		js << "]}";
		std::cout << js.str() << "\n";
		return 0;
	}
	if (cmd == "replay-svg") {
		std::string state, sstep;
		if (!get_arg(argc, argv, std::string("--state"), state) ||
		    !get_arg(argc, argv, std::string("--step"), sstep)) { usage(); return 1; }
		int target = std::stoi(sstep);
		AppState st; std::string err;
		if (!AppState::load(st, state, err)) { std::cerr << err << "\n"; return 2; }
		AppState cur; cur.map = st.base_map; cur.game = st.base_game;
		int limit = std::min(target, (int)st.log.size());
		for (int i = 0; i < limit; ++i) applyLogEntry(st.log[i], cur);
		std::string svg = render_svg(cur, 32.0f, 16.0f);
		std::cout << svg;
		return 0;
	}
	if (cmd == "init-base") {
		std::string state;
		if (!get_arg(argc, argv, std::string("--state"), state)) { usage(); return 1; }
		AppState st; std::string err;
		if (!AppState::load(st, state, err)) { std::cerr << err << "\n"; return 2; }
		st.set_base_from_current();
		if (!AppState::save(st, state, err)) { std::cerr << err << "\n"; return 2; }
		std::cout << "Базовое состояние сохранено в файл.\n";
		return 0;
	}
	if (cmd == "resolve-bots") {
		std::string state;
		if (!get_arg(argc, argv, std::string("--state"), state)) { usage(); return 1; }
		AppState st; std::string err;
		if (!AppState::load(st, state, err)) { std::cerr << err << "\n"; return 2; }
		run_pending_bot_turns(st);
		if (!AppState::save(st, state, err)) { std::cerr << err << "\n"; return 2; }
		return 0;
	}
	if (cmd == "replay-export-one") {
		std::string state, outdir, scell, smargin;
		if (!get_arg(argc, argv, std::string("--state"), state) ||
		    !get_arg(argc, argv, std::string("--out-dir"), outdir)) { usage(); return 1; }
		float cell = 36.0f, margin = 24.0f;
		if (get_arg(argc, argv, std::string("--cell"), scell)) cell = std::stof(scell);
		if (get_arg(argc, argv, std::string("--margin"), smargin)) margin = std::stof(smargin);
		AppState st; std::string err;
		if (!AppState::load(st, state, err)) { std::cerr << err << "\n"; return 2; }
		std::filesystem::create_directories(outdir);
		AppState cur; cur.map = st.base_map; cur.game = st.base_game;
		// frame 0
		{
			std::string svg = render_svg(cur, cell, margin);
			std::ofstream f(outdir + "/frame_0000.svg"); f << svg;
		}
		int step = 1;
		for (const auto& e : st.log) {
			applyLogEntry(e, cur);
			char buf[64];
			std::snprintf(buf, sizeof(buf), "/frame_%04d.svg", step++);
			std::string svg = render_svg(cur, cell, margin);
			std::ofstream f(outdir + buf); f << svg;
		}
		std::cout << "Exported " << step << " frames to " << outdir << "\n";
		return 0;
	}
	if (cmd == "set-knife") {
		std::string state, name, sbroken;
		if (!get_arg(argc, argv, std::string("--state"), state) ||
		    !get_arg(argc, argv, std::string("--name"), name) ||
		    !get_arg(argc, argv, std::string("--broken"), sbroken)) { usage(); return 1; }
		int broken = std::stoi(sbroken);
		AppState st; std::string err;
		if (!AppState::load(st, state, err)) { std::cerr << err << "\n"; return 2; }
		if (broken) st.game.broken_knife.insert(name);
		else st.game.broken_knife.erase(name);
		if (!AppState::save(st, state, err)) { std::cerr << err << "\n"; return 2; }
		std::cout << "OK\n"; return 0;
	}
	if (cmd == "set-turns") {
		std::string state;
		if (!get_arg(argc, argv, std::string("--state"), state)) { usage(); return 1; }
		if (argc < 3) { usage(); return 1; }
		int val = std::stoi(argv[argc - 1]);
		AppState st; std::string err;
		if (!AppState::load(st, state, err)) { std::cerr << err << "\n"; return 2; }
		st.game.enforce_turns = (val != 0);
		if (!AppState::save(st, state, err)) { std::cerr << err << "\n"; return 2; }
		std::cout << (st.game.enforce_turns ? "Turns ON" : "Turns OFF") << "\n";
		return 0;
	}
	usage();
	return 1;
}


