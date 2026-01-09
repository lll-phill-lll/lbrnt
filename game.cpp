#include "game.hpp"
#include "items/Item.hpp"
#include "items/Knife.hpp"
#include "items/Shotgun.hpp"
#include "items/Rifle.hpp"
#include "items/Flashlight.hpp"
#include "locations/Location.hpp"

static size_t manhattan(std::pair<size_t,size_t> a, std::pair<size_t,size_t> b) {
	size_t dx = a.first > b.first ? a.first - b.first : b.first - a.first;
	size_t dy = a.second > b.second ? a.second - b.second : b.second - a.second;
	return dx + dy;
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
	item_charges[name]["knife"] = 1;
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
			return out;
		}
	}
	if (!can) {
		out.moved = false;
		out.position = pos;
		out.messages.push_back("Врезался в стену");
		return out;
	}
	std::pair<size_t,size_t> new_pos{static_cast<size_t>(nx), static_cast<size_t>(ny)};
	// onExit for previous location if leaving it
	CellContent prevCell = map.get_cell(pos.first, pos.second);
	players[name] = new_pos;
	out.moved = true;
	out.position = new_pos;

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
			item_charges[name][itemId] += grant;
			if (itemId == "knife") {
				if (item_charges[name][itemId] > 0) broken_knife.erase(name);
			}
			if (itemId == "flashlight") out.messages.push_back("Подобран фонарь");
			else if (itemId == "rifle") out.messages.push_back("Подобрано ружьё");
			else if (itemId == "shotgun") out.messages.push_back("Подобран дробовик");
			else if (itemId == "knife") out.messages.push_back("Подобран нож");
			else out.messages.push_back("Подобран предмет: " + itemId);
		}
		ground_items.erase(itItems);
	}
	for (const auto& kv : players) {
		if (kv.first != name && manhattan(new_pos, kv.second) <= 1) {
			out.messages.push_back("Вы чувствуете чьё-то дыхание поблизости.");
			break;
		}
	}
	return out;
}

AttackOutcome Game::attack(const std::string& name, Direction dir, LabyrinthMap& map) {
	// Backward compatibility: attack = use knife
	AttackOutcome out;
	auto use = use_item(name, std::string("knife"), dir, map);
	out.attacked = use.used;
	out.messages = std::move(use.messages);
	return out;
}

UseOutcome Game::use_item(const std::string& name, const std::string& itemId, Direction dir, LabyrinthMap& map) {
	UseOutcome out;
	auto itp = players.find(name);
	if (itp == players.end()) { out.messages.push_back("Нет такого игрока"); return out; }
	// Create requested item
	std::unique_ptr<Item> item;
	if (itemId == "knife") item = std::make_unique<Knife>();
	else if (itemId == "shotgun") item = std::make_unique<Shotgun>();
	else if (itemId == "rifle") item = std::make_unique<Rifle>();
	else if (itemId == "flashlight") item = std::make_unique<Flashlight>();
	else { out.messages.push_back("Неизвестный предмет"); return out; }
	// initialize charges lazily
	int& charges = item_charges[name][item->id()];
	if (charges == 0) charges = item->defaultInitialCharges();
	int spend = item->chargesPerUse();
	if (charges < spend) {
		if (itemId == "knife") out.messages.push_back("Нож сломан");
		else out.messages.push_back("Предмет исчерпан");
		return out;
	}
	// apply effect
	item->apply(*this, map, name, dir, out.messages);
	// spend charges upon use (use may miss; still consumed)
	charges -= spend;
	// sync compatibility for knife indicator
	if (itemId == "knife") {
		if (charges <= 0) broken_knife.insert(name);
		else broken_knife.erase(name);
	}
	out.used = true;
	return out;
}


