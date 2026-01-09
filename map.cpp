#include "map.hpp"
#include <sstream>

LabyrinthMap::LabyrinthMap(size_t w, size_t h) : width(w), height(h) {
	cells.assign(height, std::vector<CellContent>(width, CellContent::Empty));
	v_walls.assign(height, std::vector<bool>(width + 1, true));
	h_walls.assign(height + 1, std::vector<bool>(width, true));
	has_exit = false;
	hospital_cells.clear();
	arsenal_cells.clear();
}

bool LabyrinthMap::in_bounds(long x, long y) const {
	return x >= 0 && y >= 0 && static_cast<size_t>(x) < width && static_cast<size_t>(y) < height;
}

CellContent LabyrinthMap::get_cell(size_t x, size_t y) const {
	return cells[y][x];
}

void LabyrinthMap::set_cell(size_t x, size_t y, CellContent c) {
	CellContent prev = cells[y][x];
	if (prev == CellContent::Hospital) {
		for (auto it = hospital_cells.begin(); it != hospital_cells.end(); ++it) {
			if (it->first == x && it->second == y) { hospital_cells.erase(it); break; }
		}
	} else if (prev == CellContent::Arsenal) {
		for (auto it = arsenal_cells.begin(); it != arsenal_cells.end(); ++it) {
			if (it->first == x && it->second == y) { arsenal_cells.erase(it); break; }
		}
	}
	cells[y][x] = c;
	if (c == CellContent::Hospital) {
		hospital_cells.emplace_back(x, y);
	} else if (c == CellContent::Arsenal) {
		arsenal_cells.emplace_back(x, y);
	}
}

void LabyrinthMap::set_vwall(size_t y, size_t x, bool present) {
	v_walls[y][x] = present;
}

void LabyrinthMap::set_hwall(size_t y, size_t x, bool present) {
	h_walls[y][x] = present;
}

bool LabyrinthMap::can_move_left(size_t x, size_t y) const {
	return x > 0 && !v_walls[y][x];
}
bool LabyrinthMap::can_move_right(size_t x, size_t y) const {
	return x + 1 < width && !v_walls[y][x + 1];
}
bool LabyrinthMap::can_move_up(size_t x, size_t y) const {
	return y > 0 && !h_walls[y][x];
}
bool LabyrinthMap::can_move_down(size_t x, size_t y) const {
	return y + 1 < height && !h_walls[y + 1][x];
}

std::string cell_to_char(CellContent c, bool reveal) {
	switch (c) {
		case CellContent::Empty: return reveal ? "." : " ";
		case CellContent::Treasure: return "T";
		case CellContent::Hospital: return "H";
		case CellContent::Arsenal: return "A";
		case CellContent::Exit: return "E";
	}
	return " ";
}

std::string LabyrinthMap::render_ascii(const std::unordered_map<std::string, std::pair<size_t,size_t>>* players, bool reveal, const std::unordered_map<long long,int>* loot_treasure) const {
	std::unordered_map<long long, std::vector<char>> pos_to_labels;
	if (players) {
		for (const auto& kv : *players) {
			char ch = kv.first.empty() ? 'P' : static_cast<char>(::toupper(kv.first[0]));
			long long key = static_cast<long long>(kv.second.second) * 1000000LL + static_cast<long long>(kv.second.first);
			pos_to_labels[key].push_back(ch);
		}
	}
	std::ostringstream oss;
	// top edge
	for (size_t x = 0; x < width; ++x) {
		oss << "+";
		if (is_exit_edge_horizontal(0, x)) {
			oss << "E";
		} else {
			oss << (h_walls[0][x] ? "-" : " ");
		}
	}
	oss << "+\n";
	for (size_t y = 0; y < height; ++y) {
		for (size_t x = 0; x < width; ++x) {
			if (is_exit_edge_vertical(y, x)) {
				oss << "E";
			} else {
				oss << (v_walls[y][x] ? "|" : " ");
			}
			long long key = static_cast<long long>(y) * 1000000LL + static_cast<long long>(x);
			if (pos_to_labels.count(key)) {
				if (pos_to_labels[key].size() == 1) {
					oss << pos_to_labels[key][0];
				} else {
					oss << "*";
				}
			} else {
				// overlay loot treasure always if present
				bool lootT = loot_treasure && loot_treasure->count(key) && loot_treasure->at(key) > 0;
				if (lootT) oss << "T";
				else oss << cell_to_char(get_cell(x, y), reveal);
			}
		}
		if (is_exit_edge_vertical(y, width)) {
			oss << "E\n";
		} else {
			oss << (v_walls[y][width] ? "|\n" : " \n");
		}
		for (size_t x = 0; x < width; ++x) {
			oss << "+";
			if (is_exit_edge_horizontal(y + 1, x)) {
				oss << "E";
			} else {
				oss << (h_walls[y + 1][x] ? "-" : " ");
			}
		}
		oss << "+\n";
	}
	return oss.str();
}


