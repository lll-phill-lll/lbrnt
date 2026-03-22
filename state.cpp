#include "state.hpp"
#include "rng.hpp"
#include <fstream>
#include <sstream>
#include <random>
#include <unordered_map>
#include "locations/Location.hpp"

static const char* dir_to_cstr(Direction d) {
	switch (d) {
		case Direction::Up: return "up";
		case Direction::Down: return "down";
		case Direction::Left: return "left";
		case Direction::Right: return "right";
	}
	return "up";
}
static Direction cstr_to_dir(const std::string& s) {
	if (s=="up") return Direction::Up;
	if (s=="down") return Direction::Down;
	if (s=="left") return Direction::Left;
	if (s=="right") return Direction::Right;
	return Direction::Up;
}

static std::string cell_char(CellContent c) {
	switch (c) {
		case CellContent::Empty: return ".";
		case CellContent::Treasure: return "T";
		case CellContent::Hospital: return "H";
		case CellContent::Arsenal: return "A";
		case CellContent::Exit: return "E";
	}
	return ".";
}
static CellContent from_cell_char(char c) {
	switch (c) {
		case 'T': return CellContent::Treasure;
		case 'H': return CellContent::Hospital;
		case 'A': return CellContent::Arsenal;
		case 'E': return CellContent::Exit;
		default: return CellContent::Empty;
	}
}

bool AppState::save(const AppState& st, const std::string& path, std::string& err) {
	std::ofstream f(path);
	if (!f) { err = "Не могу открыть файл для записи"; return false; }
	// ensure base exists
	AppState copy = st;
	if (copy.base_map.width == 0 || copy.base_map.height == 0) {
		copy.set_base_from_current();
	}
	f << st.map.width << " " << st.map.height << "\n";
	f << "VWALLS\n";
	for (size_t y = 0; y < st.map.height; ++y) {
		for (size_t x = 0; x <= st.map.width; ++x) {
			f << (st.map.v_walls[y][x] ? '1' : '0');
			if (x < st.map.width) f << " ";
		}
		f << "\n";
	}
	f << "HWALLS\n";
	for (size_t y = 0; y <= st.map.height; ++y) {
		for (size_t x = 0; x < st.map.width; ++x) {
			f << (st.map.h_walls[y][x] ? '1' : '0');
			if (x + 1 < st.map.width) f << " ";
		}
		f << "\n";
	}
	f << "CELLS\n";
	for (size_t y = 0; y < st.map.height; ++y) {
		for (size_t x = 0; x < st.map.width; ++x) {
			f << cell_char(st.map.get_cell(x, y));
			if (x + 1 < st.map.width) f << " ";
		}
		f << "\n";
	}
	f << "EXIT ";
	if (st.map.has_exit) {
		f << (st.map.exit_vertical ? "V " : "H ") << st.map.exit_y << " " << st.map.exit_x << "\n";
	} else {
		f << "NONE\n";
	}
	// RNG state
	f << "RNG " << st.random_seed << " " << st.random_nonce << "\n";
	f << "PLAYERS " << st.game.players.size() << "\n";
	for (const auto& kv : st.game.players) {
		int has_t = player_has_treasure(st.game, kv.first) ? 1 : 0;
		int broken = st.game.broken_knife.count(kv.first) ? 1 : 0;
		f << kv.first << " " << kv.second.first << " " << kv.second.second << " " << has_t << " " << broken << "\n";
	}
	// Turns
	f << "TURNS " << (st.game.enforce_turns ? 1 : 0) << " " << st.game.turn_index << " " << st.game.turn_order.size() << "\n";
	for (const auto& n : st.game.turn_order) f << n << "\n";
	f << "TURNRNG " << st.game.turn_rng_state << "\n";
	// Actions
	f << "ACTIONS " << st.game.actions_per_turn << " " << st.game.actions_left << "\n";
	// Bot
	f << "BOT " << (st.game.bot_enabled?1:0) << " " << st.game.bot_x << " " << st.game.bot_y << " " << st.game.bot_steps_per_turn << "\n";
	// per-player item charges
	size_t total_items = 0;
	for (const auto& pkv : st.game.inventories) total_items += pkv.second.item_charges.size();
	f << "ITEMS " << total_items << "\n";
	for (const auto& pkv : st.game.inventories) {
		for (const auto& iv : pkv.second.item_charges) {
			f << pkv.first << " " << iv.first << " " << iv.second << "\n";
		}
	}
	f << "PCOLORS " << st.game.player_color.size() << "\n";
	for (const auto& kv : st.game.player_color) {
		f << kv.first << " " << kv.second << "\n";
	}
	f << "LOOT_T " << st.game.loot_treasure.size() << "\n";
	for (const auto& kv : st.game.loot_treasure) {
		long long key = kv.first; int c = kv.second;
		size_t y = (size_t)(key / 1000000LL);
		size_t x = (size_t)(key % 1000000LL);
		f << x << " " << y << " " << c << "\n";
	}
	// ground items (x y id charges)
	{
		size_t gi_count = 0;
		for (const auto& kv : st.game.ground_items) gi_count += kv.second.size();
		f << "LOOT_I " << gi_count << "\n";
		for (const auto& kv : st.game.ground_items) {
			size_t y = (size_t)(kv.first / 1000000LL);
			size_t x = (size_t)(kv.first % 1000000LL);
			for (const auto& iv : kv.second) {
				f << x << " " << y << " " << iv.first << " " << iv.second << "\n";
			}
		}
	}
	f << "LOG " << st.log.size() << "\n";
	for (const auto& e : st.log) {
		switch (e.type) {
			case LogType::Move:
				f << "MOVE " << e.name << " " << dir_to_cstr(e.dir) << "\n"; break;
			case LogType::Attack:
				f << "ATTACK " << e.name << " " << dir_to_cstr(e.dir) << "\n"; break;
			case LogType::UseItem:
				f << "USE " << e.name << " " << e.item << " " << dir_to_cstr(e.dir) << "\n"; break;
			case LogType::AddPlayer:
				f << "ADD " << e.name << " " << e.x << " " << e.y << "\n"; break;
			case LogType::AddPlayerRandom:
				f << "ADDR " << e.name << " " << e.x << " " << e.y << "\n"; break;
			case LogType::BotMove:
				f << "BOTMOVE " << e.x << " " << e.y << "\n"; break;
			case LogType::BotKill:
				f << "BOTKILL " << e.name << "\n"; break;
		}
	}
	f << "FINISHED " << (st.game.finished ? 1 : 0) << "\n";
	// BASE block appended at end (always present)
		f << "BASE\n";
		f << "BWH " << copy.base_map.width << " " << copy.base_map.height << "\n";
		f << "BVWALLS\n";
		for (size_t y = 0; y < copy.base_map.height; ++y) {
			for (size_t x = 0; x <= copy.base_map.width; ++x) {
				f << (copy.base_map.v_walls[y][x] ? '1' : '0');
				if (x < copy.base_map.width) f << " ";
			}
			f << "\n";
		}
		f << "BHWALLS\n";
		for (size_t y = 0; y <= copy.base_map.height; ++y) {
			for (size_t x = 0; x < copy.base_map.width; ++x) {
				f << (copy.base_map.h_walls[y][x] ? '1' : '0');
				if (x + 1 < copy.base_map.width) f << " ";
			}
			f << "\n";
		}
		f << "BCELLS\n";
		for (size_t y = 0; y < copy.base_map.height; ++y) {
			for (size_t x = 0; x < copy.base_map.width; ++x) {
				f << cell_char(copy.base_map.get_cell(x, y));
				if (x + 1 < copy.base_map.width) f << " ";
			}
			f << "\n";
		}
		f << "BEXIT ";
		if (copy.base_map.has_exit) {
			f << (copy.base_map.exit_vertical ? "V " : "H ") << copy.base_map.exit_y << " " << copy.base_map.exit_x << "\n";
		} else {
			f << "NONE\n";
		}
		f << "BPLAYERS " << copy.base_game.players.size() << "\n";
		for (const auto& kv : copy.base_game.players) {
			int has_t = player_has_treasure(copy.base_game, kv.first) ? 1 : 0;
			int broken = copy.base_game.broken_knife.count(kv.first) ? 1 : 0;
			f << kv.first << " " << kv.second.first << " " << kv.second.second << " " << has_t << " " << broken << "\n";
		}
		f << "BTURNS " << (copy.base_game.enforce_turns ? 1 : 0) << " " << copy.base_game.turn_index << " " << copy.base_game.turn_order.size() << "\n";
		for (const auto& n : copy.base_game.turn_order) f << n << "\n";
		f << "BTURNRNG " << copy.base_game.turn_rng_state << "\n";
		f << "BACTIONS " << copy.base_game.actions_per_turn << " " << copy.base_game.actions_left << "\n";
		f << "BBOT " << (copy.base_game.bot_enabled?1:0) << " " << copy.base_game.bot_x << " " << copy.base_game.bot_y << " " << copy.base_game.bot_steps_per_turn << "\n";
		f << "BPCOLORS " << copy.base_game.player_color.size() << "\n";
		for (const auto& kv : copy.base_game.player_color) {
			f << kv.first << " " << kv.second << "\n";
		}
		// base per-player items
		size_t b_total_items = 0;
		for (const auto& pkv : copy.base_game.inventories) b_total_items += pkv.second.item_charges.size();
		f << "BITEMS " << b_total_items << "\n";
		for (const auto& pkv : copy.base_game.inventories) {
			for (const auto& iv : pkv.second.item_charges) {
				f << pkv.first << " " << iv.first << " " << iv.second << "\n";
			}
		}
		f << "BLOOT_T " << copy.base_game.loot_treasure.size() << "\n";
		for (const auto& kv : copy.base_game.loot_treasure) {
			long long key = kv.first; int c = kv.second;
			size_t y = (size_t)(key / 1000000LL);
			size_t x = (size_t)(key % 1000000LL);
			f << x << " " << y << " " << c << "\n";
		}
		// base ground items
		{
			size_t bgi = 0; for (const auto& kv : copy.base_game.ground_items) bgi += kv.second.size();
			f << "BLOOT_I " << bgi << "\n";
			for (const auto& kv : copy.base_game.ground_items) {
				size_t y = (size_t)(kv.first / 1000000LL);
				size_t x = (size_t)(kv.first % 1000000LL);
				for (const auto& iv : kv.second) {
					f << x << " " << y << " " << iv.first << " " << iv.second << "\n";
				}
			}
		}
		f << "BASE_END\n";
	return true;
}

bool AppState::load(AppState& st, const std::string& path, std::string& err) {
	std::ifstream f(path);
	if (!f) { err = "Не могу открыть файл для чтения"; return false; }
	size_t w, h;
	if (!(f >> w >> h)) { err = "Некорректный заголовок"; return false; }
	st.map = LabyrinthMap(w, h);
	std::string token;
	if (!(f >> token) || token != "VWALLS") { err = "Ожидался VWALLS"; return false; }
	for (size_t y = 0; y < h; ++y) {
		for (size_t x = 0; x <= w; ++x) {
			char c; f >> c;
			st.map.v_walls[y][x] = (c == '1');
		}
	}
	if (!(f >> token) || token != "HWALLS") { err = "Ожидался HWALLS"; return false; }
	for (size_t y = 0; y <= h; ++y) {
		for (size_t x = 0; x < w; ++x) {
			char c; f >> c;
			st.map.h_walls[y][x] = (c == '1');
		}
	}
	if (!(f >> token) || token != "CELLS") { err = "Ожидался CELLS"; return false; }
	for (size_t y = 0; y < h; ++y) {
		for (size_t x = 0; x < w; ++x) {
			std::string s; f >> s;
			st.map.set_cell(x, y, s.empty() ? CellContent::Empty : from_cell_char(s[0]));
		}
	}
	if (!(f >> token) || token != "EXIT") { err = "Ожидался EXIT"; return false; }
	std::string ex;
	if (!(f >> ex)) { err = "Некорректный EXIT"; return false; }
	if (ex == "NONE") {
		st.map.has_exit = false;
	} else {
		st.map.has_exit = true;
		if (ex == "V") st.map.exit_vertical = true; else if (ex == "H") st.map.exit_vertical = false; else { err = "Некорректный тип EXIT"; return false; }
		if (!(f >> st.map.exit_y >> st.map.exit_x)) { err = "Некорректные координаты EXIT"; return false; }
	}
	// Optional RNG
	if (!(f >> token)) { err = "Ожидался PLAYERS или RNG"; return false; }
	if (token == "RNG") {
		unsigned int seed = 0; unsigned long long nonce = 0;
		if (!(f >> seed >> nonce)) { err = "Некорректный RNG"; return false; }
		st.random_seed = seed; st.random_nonce = nonce;
		st.game.turn_rng_state = game_rng::initial_turn_rng_state(st.random_seed);
		if (!(f >> token)) { err = "Ожидался PLAYERS"; return false; }
		// Совместимость: старые файлы с необязательной строкой OPENNESS (игнорируем).
		if (token == "OPENNESS") {
			float skip_op = 0.f;
			if (!(f >> skip_op)) { err = "Некорректный OPENNESS"; return false; }
			if (!(f >> token)) { err = "Ожидался PLAYERS"; return false; }
		}
	} else {
		// default RNG if absent
		st.random_seed = std::random_device{}();
		st.random_nonce = 0;
		st.game.turn_rng_state = game_rng::initial_turn_rng_state(st.random_seed);
	}
	/** Флаг has_t из старых сохранений; после ITEMS мигрируем в charges предмета `treasure`. */
	std::unordered_map<std::string, bool> legacy_player_treasure;
	std::unordered_map<std::string, bool> legacy_base_treasure;
	size_t nplayers = 0;
	if (token != "PLAYERS") { err = "Ожидался PLAYERS"; return false; }
	if (!(f >> nplayers)) { err = "Некорректное число игроков"; return false; }
	st.game.players.clear();
	st.game.broken_knife.clear();
	for (size_t i = 0; i < nplayers; ++i) {
		std::string name; size_t px, py; int has_t; int broken = 0;
		f >> name >> px >> py >> has_t >> broken;
		st.game.players[name] = {px, py};
		legacy_player_treasure[name] = (has_t != 0);
		if (broken) st.game.broken_knife.insert(name);
	}
	// Очистить инвентари перед разбором optional-секций и миграцией legacy has_t.
	st.game.inventories.clear();
	// Optional turns/actions/items/colors sections
	if (!(f >> token)) { err = "Ожидался FINISHED или PCOLORS/ITEMS"; return false; }
	if (token == "TURNS") {
		int enf=0; size_t idx=0, cnt=0;
		if (!(f >> enf >> idx >> cnt)) { err = "Некорректный TURNS"; return false; }
		st.game.enforce_turns = (enf!=0);
		st.game.turn_index = idx;
		st.game.turn_order.clear();
		for (size_t i=0;i<cnt;++i) { std::string n; f >> n; st.game.turn_order.push_back(n); }
		if (!(f >> token)) { err = "Ожидался FINISHED или TURNRNG/ACTIONS/PCOLORS/ITEMS"; return false; }
		if (token == "TURNRNG") {
			unsigned long long tr = 0;
			if (!(f >> tr)) { err = "Некорректный TURNRNG"; return false; }
			st.game.turn_rng_state = tr;
			if (!(f >> token)) { err = "Ожидался FINISHED или ACTIONS/PCOLORS/ITEMS"; return false; }
		}
	}
	if (token == "ACTIONS") {
		int per=1, left=1;
		if (!(f >> per >> left)) { err = "Некорректный ACTIONS"; return false; }
		st.game.actions_per_turn = per;
		st.game.actions_left = left;
		if (!(f >> token)) { err = "Ожидался FINISHED или BOT/PCOLORS/ITEMS"; return false; }
	}
	if (token == "BOT") {
		int en=0; size_t bx=0, by=0; int steps=1;
		if (!(f >> en >> bx >> by >> steps)) { err = "Некорректный BOT"; return false; }
		st.game.bot_enabled = (en!=0);
		st.game.bot_x = bx; st.game.bot_y = by; st.game.bot_steps_per_turn = steps;
		if (!(f >> token)) { err = "Ожидался FINISHED или PCOLORS/ITEMS"; return false; }
	}
	if (token == "ITEMS") {
		size_t k = 0; if (!(f >> k)) { err = "Некорректный ITEMS"; return false; }
		st.game.inventories.clear();
		for (size_t i = 0; i < k; ++i) {
			std::string pname, item; int c; f >> pname >> item >> c;
			st.game.inventories[pname].setCharges(item, c);
		}
		if (!(f >> token)) { err = "Ожидался FINISHED или PCOLORS/LOOT_T"; return false; }
	}
	for (const auto& kv : legacy_player_treasure) {
		if (!kv.second) continue;
		auto& inv = st.game.inventories[kv.first];
		if (inv.getCharges("treasure") <= 0) inv.setCharges("treasure", 1);
	}
	if (token == "PCOLORS") {
		size_t m = 0; if (!(f >> m)) { err = "Некорректный PCOLORS"; return false; }
		st.game.player_color.clear();
		for (size_t i = 0; i < m; ++i) {
			std::string name, color; f >> name >> color;
			st.game.player_color[name] = color;
		}
		if (!(f >> token)) { err = "Ожидался FINISHED или LOOT_T"; return false; }
	}
	// Optional loot treasures
	if (token == "LOOT_T") {
		size_t k = 0; if (!(f >> k)) { err = "Некорректный LOOT_T"; return false; }
		st.game.loot_treasure.clear();
		for (size_t i = 0; i < k; ++i) {
			size_t x, y; int c; f >> x >> y >> c;
			long long key = (long long)y * 1000000LL + (long long)x;
			st.game.loot_treasure[key] = c;
		}
		if (!(f >> token)) { err = "Ожидался FINISHED или LOOT_I/LOG"; return false; }
	}
	if (token == "LOOT_I") {
		size_t k = 0; if (!(f >> k)) { err = "Некорректный LOOT_I"; return false; }
		st.game.ground_items.clear();
		for (size_t i = 0; i < k; ++i) {
			size_t x, y; std::string item; int c;
			f >> x >> y >> item >> c;
			long long key = (long long)y * 1000000LL + (long long)x;
			st.game.ground_items[key][item] = c;
		}
		if (!(f >> token)) { err = "Ожидался FINISHED или LOG"; return false; }
	}
	if (token == "LOG") {
		size_t n = 0; if (!(f >> n)) { err = "Некорректный LOG"; return false; }
		st.log.clear();
		for (size_t i = 0; i < n; ++i) {
			std::string kind; f >> kind;
			if (kind == "MOVE" || kind == "ATTACK") {
				std::string name, sdir; f >> name >> sdir;
				LogEntry e; e.type = (kind=="MOVE") ? LogType::Move : LogType::Attack; e.name = name; e.dir = cstr_to_dir(sdir);
				st.log.push_back(e);
			} else if (kind == "USE") {
				std::string name, item, sdir; f >> name >> item >> sdir;
				LogEntry e; e.type = LogType::UseItem; e.name = name; e.item = item; e.dir = cstr_to_dir(sdir);
				st.log.push_back(e);
			} else if (kind == "ADD" || kind == "ADDR") {
				std::string name; size_t x, y; f >> name >> x >> y;
				LogEntry e; e.type = (kind=="ADD") ? LogType::AddPlayer : LogType::AddPlayerRandom; e.name = name; e.x = x; e.y = y;
				st.log.push_back(e);
			} else if (kind == "BOTMOVE") {
				size_t x, y; f >> x >> y;
				LogEntry e; e.type = LogType::BotMove; e.x = x; e.y = y;
				st.log.push_back(e);
			} else if (kind == "BOTKILL") {
				std::string name; f >> name;
				LogEntry e; e.type = LogType::BotKill; e.name = name;
				st.log.push_back(e);
			} else {
				err = "Неизвестная запись лога"; return false;
			}
		}
		if (!(f >> token)) { err = "Ожидался FINISHED"; return false; }
	}
	if (token != "FINISHED") { err = "Ожидался FINISHED"; return false; }
	int fin; f >> fin; st.game.finished = (fin != 0);
	// BASE block after FINISHED (if none, base = current)
	std::string t2;
	if (f >> t2) {
		if (t2 == "BASE") {
			std::string btoken;
			size_t bw = 0, bh = 0;
			if (!(f >> btoken) || btoken != "BWH") { err = "Ожидался BWH"; return false; }
			if (!(f >> bw >> bh)) { err = "Некорректный BWH"; return false; }
			st.base_map = LabyrinthMap(bw, bh);
			if (!(f >> btoken) || btoken != "BVWALLS") { err = "Ожидался BVWALLS"; return false; }
			for (size_t y = 0; y < bh; ++y) for (size_t x = 0; x <= bw; ++x) { char c; f >> c; st.base_map.v_walls[y][x] = (c=='1'); }
			if (!(f >> btoken) || btoken != "BHWALLS") { err = "Ожидался BHWALLS"; return false; }
			for (size_t y = 0; y <= bh; ++y) for (size_t x = 0; x < bw; ++x) { char c; f >> c; st.base_map.h_walls[y][x] = (c=='1'); }
			if (!(f >> btoken) || btoken != "BCELLS") { err = "Ожидался BCELLS"; return false; }
			for (size_t y = 0; y < bh; ++y) for (size_t x = 0; x < bw; ++x) { std::string s; f >> s; st.base_map.set_cell(x, y, s.empty()?CellContent::Empty:from_cell_char(s[0])); }
			if (!(f >> btoken) || btoken != "BEXIT") { err = "Ожидался BEXIT"; return false; }
			std::string bex; if (!(f >> bex)) { err = "Некорректный BEXIT"; return false; }
			if (bex == "NONE") { st.base_map.has_exit = false; }
			else { st.base_map.has_exit = true; st.base_map.exit_vertical = (bex=="V"); if (!(f >> st.base_map.exit_y >> st.base_map.exit_x)) { err = "Некорректные BEXIT координаты"; return false; } }
			size_t bn = 0;
			if (!(f >> btoken) || btoken != "BPLAYERS") { err = "Ожидался BPLAYERS"; return false; }
			if (!(f >> bn)) { err = "Некорректный BPLAYERS"; return false; }
			st.base_game = Game{};
			for (size_t i = 0; i < bn; ++i) {
				std::string name; size_t px, py; int ht, br;
				f >> name >> px >> py >> ht >> br;
				st.base_game.players[name] = {px, py};
				legacy_base_treasure[name] = (ht != 0);
				if (br) st.base_game.broken_knife.insert(name);
			}
			if (!(f >> btoken)) { err = "Ожидался BTURNS или BPCOLORS"; return false; }
			if (btoken == "BTURNS") {
				int enf=0; size_t idx=0, cnt=0; if (!(f >> enf >> idx >> cnt)) { err = "Некорректный BTURNS"; return false; }
				st.base_game.enforce_turns = (enf!=0);
				st.base_game.turn_index = idx;
				st.base_game.turn_rng_state = game_rng::initial_turn_rng_state(st.random_seed);
				for (size_t i=0;i<cnt;++i) { std::string n; f >> n; st.base_game.turn_order.push_back(n); }
				if (!(f >> btoken)) { err = "Ожидался BTURNRNG или BACTIONS или BPCOLORS"; return false; }
				if (btoken == "BTURNRNG") {
					unsigned long long btr = 0;
					if (!(f >> btr)) { err = "Некорректный BTURNRNG"; return false; }
					st.base_game.turn_rng_state = btr;
					if (!(f >> btoken)) { err = "Ожидался BACTIONS или BPCOLORS"; return false; }
				}
			}
			if (btoken == "BACTIONS") {
				int per=1, left=1; if (!(f >> per >> left)) { err = "Некорректный BACTIONS"; return false; }
				st.base_game.actions_per_turn = per;
				st.base_game.actions_left = left;
				if (!(f >> btoken)) { err = "Ожидался BBOT или BPCOLORS"; return false; }
			}
			if (btoken == "BBOT") {
				int en=0; size_t bx=0, by=0; int steps=1;
				if (!(f >> en >> bx >> by >> steps)) { err = "Некорректный BBOT"; return false; }
				st.base_game.bot_enabled = (en!=0);
				st.base_game.bot_x = bx; st.base_game.bot_y = by; st.base_game.bot_steps_per_turn = steps;
				if (!(f >> btoken)) { err = "Ожидался BPCOLORS"; return false; }
			}
			size_t bm=0;
			if (btoken != "BPCOLORS") { err = "Ожидался BPCOLORS"; return false; }
			if (!(f >> bm)) { err = "Некорректный BPCOLORS"; return false; }
			for (size_t i = 0; i < bm; ++i) { std::string name, color; f >> name >> color; st.base_game.player_color[name]=color; }
			// optional base items
			if (!(f >> btoken)) { err = "Ожидался BITEMS или BLOOT_T"; return false; }
			if (btoken == "BITEMS") {
				size_t bi=0; if (!(f >> bi)) { err = "Некорректный BITEMS"; return false; }
				for (size_t i = 0; i < bi; ++i) {
					std::string pname, item; int c; f >> pname >> item >> c; st.base_game.inventories[pname].setCharges(item, c);
				}
				if (!(f >> btoken)) { err = "Ожидался BLOOT_T"; return false; }
			}
			for (const auto& kv : legacy_base_treasure) {
				if (!kv.second) continue;
				auto& inv = st.base_game.inventories[kv.first];
				if (inv.getCharges("treasure") <= 0) inv.setCharges("treasure", 1);
			}
			size_t bk=0;
			if (btoken != "BLOOT_T") { err = "Ожидался BLOOT_T"; return false; }
			if (!(f >> bk)) { err = "Некорректный BLOOT_T"; return false; }
			for (size_t i = 0; i < bk; ++i) { size_t x,y; int c; f >> x >> y >> c; long long key=(long long)y*1000000LL+(long long)x; st.base_game.loot_treasure[key]=c; }
			if (!(f >> btoken)) { err = "Ожидался BLOOT_I или BASE_END"; return false; }
			if (btoken == "BLOOT_I") {
				size_t bi=0; if (!(f >> bi)) { err = "Некорректный BLOOT_I"; return false; }
				for (size_t i = 0; i < bi; ++i) {
					size_t x,y; std::string item; int c; f >> x >> y >> item >> c;
					long long key=(long long)y*1000000LL+(long long)x; st.base_game.ground_items[key][item]=c;
				}
				if (!(f >> btoken)) { err = "Ожидался BASE_END"; return false; }
			}
			if (btoken != "BASE_END") { err = "Ожидался BASE_END"; return false; }
		} else {
			// token read but not BASE: ignore and set base = current
			st.base_map = st.map;
			st.base_game = st.game;
		}
	} else {
		// no token: set base = current
		st.base_map = st.map;
		st.base_game = st.game;
	}
	// Очередь всегда [игроки…, bot]; иначе в файле могло остаться [bot, игрок] → в UI «всегда ходит бот»
	st.game.canonicalize_turn_order();
	return true;
}


