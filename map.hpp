#pragma once
#include <string>
#include <vector>
#include <unordered_map>

enum class CellContent { Empty, Treasure, Hospital, Arsenal, Exit };

struct LabyrinthMap {
	size_t width{0}, height{0};
	std::vector<std::vector<CellContent>> cells;   // [h][w]
	std::vector<std::vector<bool>> v_walls;        // [h][w+1]
	std::vector<std::vector<bool>> h_walls;        // [h+1][w]
	// Exit edge on outer border
	bool has_exit{false};
	bool exit_vertical{false}; // true: vertical edge (x==0 or x==width), false: horizontal (y==0 or y==height)
	size_t exit_y{0}; // for vertical: y in [0..height-1]; for horizontal: y in {0,height}
	size_t exit_x{0}; // for vertical: x in {0,width}; for horizontal: x in [0..width-1]

	LabyrinthMap() = default;
	LabyrinthMap(size_t w, size_t h);

	bool in_bounds(long x, long y) const;

	CellContent get_cell(size_t x, size_t y) const;
	void set_cell(size_t x, size_t y, CellContent c);
	void set_vwall(size_t y, size_t x, bool present);
	void set_hwall(size_t y, size_t x, bool present);

	bool can_move_left(size_t x, size_t y) const;
	bool can_move_right(size_t x, size_t y) const;
	bool can_move_up(size_t x, size_t y) const;
	bool can_move_down(size_t x, size_t y) const;

	std::string render_ascii(const std::unordered_map<std::string, std::pair<size_t,size_t>>* players, bool reveal, const std::unordered_map<long long,int>* loot_treasure = nullptr) const;
	bool is_exit_edge_vertical(size_t y, size_t x) const { return has_exit && exit_vertical && exit_y == y && exit_x == x; }
	bool is_exit_edge_horizontal(size_t y, size_t x) const { return has_exit && !exit_vertical && exit_y == y && exit_x == x; }
};

std::string cell_to_char(CellContent c, bool reveal);


