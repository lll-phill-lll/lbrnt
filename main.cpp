#include "generator.hpp"
#include "state.hpp"
#include "viz.hpp"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>

// Simple stderr logger for service messages
static void log_err(const std::string& msg) {
	std::cerr << msg << "\n";
}

// User-facing formatted output: [Player]: then tab-indented messages
static void print_user_messages(const std::string& player, const std::vector<std::string>& messages) {
	std::cout << "[" << player << "]:" << "\n";
	for (const auto& m : messages) {
		std::cout << "\t" << m << "\n";
	}
}

static void usage() {
	std::cout <<
R"(labyrinth_cpp commands:
  generate --width W --height H --out state.txt [--openness 0..1] [--seed N]
            [--turns 0|1]
  show --state state.txt [--reveal]
  status --state state.txt
  add-player --state state.txt --name NAME --x X --y Y
  add-player-random --state state.txt --name NAME
  move --state state.txt --name NAME (up|down|left|right)
  attack --state state.txt --name NAME (up|down|left|right)
  use-item --state state.txt --name NAME --item (knife|shotgun|rifle|flashlight) (up|down|left|right)
  set-cell --state state.txt --x X --y Y (empty|treasure|hospital|arsenal|exit)
  set-vwall --state state.txt --x X --y Y (--present=0|1)
  set-hwall --state state.txt --x X --y Y (--present=0|1)
  set-knife --state state.txt --name NAME (--broken=0|1)
  add-item --state state.txt --item (knife|shotgun|rifle|flashlight) --x X --y Y [--charges N]
  add-item-random --state state.txt --item (knife|shotgun|rifle|flashlight) [--charges N]
  save-as --state state.txt --out other.txt
  export-svg --state state.txt --out maze.svg [--cell N] [--margin PX]
  replay-export --base base.txt --log state_with_log.txt --out-dir frames --cell N --margin PX
  init-base --state state.txt
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
		std::string sw, sh, out, so, sseed, sturns;
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
		st.map = generate_maze_with_items(w, h, openness);
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
				static const char* order[] = {"knife","rifle","shotgun","flashlight"};
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
					if (kv.first=="knife" || kv.first=="rifle" || kv.first=="shotgun" || kv.first=="flashlight") continue;
					lines.push_back(kv.first + ": " + std::to_string(kv.second));
				}
				if (lines.empty()) lines.push_back("Inventory: (empty)");
			}
			print_user_messages(name, lines);
		}
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
		st.log.push_back(LogEntry{LogType::AddPlayer, name, Direction::Up, (size_t)std::stoul(sx), (size_t)std::stoul(sy)});
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
		auto pos = spots[rng_pick(st, spots.size())];
		std::string e;
		if (!st.game.add_player(name, pos, st.map, e)) { std::cerr << e << "\n"; return 3; }
		st.log.push_back(LogEntry{LogType::AddPlayerRandom, name, Direction::Up, pos.first, pos.second});
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
		st.log.push_back(LogEntry{LogType::Move, name, dir, 0, 0});
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
		if (!AppState::save(st, state, err)) { std::cerr << err << "\n"; return 2; }
		// stderr detailed log
		{
			std::ostringstream es;
			es << "USE " << name << " item=" << item << " dir=" << sdir << (out.used ? " [applied]" : " [failed]");
			log_err(es.str());
		}
		// stdout user messages
		print_user_messages(name, out.messages);
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
		if (item!="knife" && item!="shotgun" && item!="rifle" && item!="flashlight") { usage(); return 1; }
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
		if (item!="knife" && item!="shotgun" && item!="rifle" && item!="flashlight") { usage(); return 1; }
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
		st.log.push_back(LogEntry{LogType::Attack, name, dir, 0, 0});
		if (!AppState::save(st, state, err)) { std::cerr << err << "\n"; return 2; }
		// stderr detailed log
		{
			std::ostringstream es;
			es << "ATTACK " << name << " dir=" << sdir << (out.attacked ? " [done]" : " [failed]");
			log_err(es.str());
		}
		// stdout user messages
		print_user_messages(name, out.messages);
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
			switch (e.type) {
				case LogType::AddPlayer: {
					std::string eerr;
					cur.game.add_player(e.name, {e.x, e.y}, cur.map, eerr);
					break;
				}
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
			}
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
			switch (e.type) {
				case LogType::AddPlayer: {
					std::string eerr;
					cur.game.add_player(e.name, {e.x, e.y}, cur.map, eerr);
					break;
				}
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
			}
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
	usage();
	return 1;
}


