#include "LocationUtils.hpp"
#include "../map.hpp"
#include <queue>
#include <tuple>
#include <algorithm>

namespace LocationUtils {

size_t count_components(const LabyrinthMap& m) {
	std::vector<std::vector<bool>> vis(m.height, std::vector<bool>(m.width, false));
	auto enqueue = [&](std::queue<std::pair<size_t,size_t>>& q, size_t x, size_t y) {
		if (!vis[y][x]) { vis[y][x] = true; q.push({x,y}); }
	};
	size_t comps = 0;
	for (size_t y = 0; y < m.height; ++y) {
		for (size_t x = 0; x < m.width; ++x) {
			if (vis[y][x]) continue;
			comps++;
			std::queue<std::pair<size_t,size_t>> q;
			enqueue(q, x, y);
			while (!q.empty()) {
				auto [cx, cy] = q.front(); q.pop();
				if (m.can_move_left(cx, cy)) enqueue(q, cx - 1, cy);
				if (m.can_move_right(cx, cy)) enqueue(q, cx + 1, cy);
				if (m.can_move_up(cx, cy)) enqueue(q, cx, cy - 1);
				if (m.can_move_down(cx, cy)) enqueue(q, cx, cy + 1);
			}
		}
	}
	return comps;
}

static bool keyIn(const std::vector<std::pair<size_t,size_t>>& cells, size_t x, size_t y) {
	for (auto& c : cells) if (c.first==x && c.second==y) return true;
	return false;
}

bool pick_and_place_location_cluster(
	LabyrinthMap& map,
	CellContent type,
	const std::vector<std::vector<std::pair<int,int>>>& patterns,
	std::mt19937& gen,
	std::vector<std::pair<size_t,size_t>>& out_cells
) {
	if (patterns.empty()) return false;
	std::vector<size_t> idx(patterns.size());
	for (size_t i=0;i<idx.size();++i) idx[i]=i;
	std::shuffle(idx.begin(), idx.end(), gen);
	for (size_t pi : idx) {
		const auto& pat = patterns[pi];
		int max_dx = 0, max_dy = 0;
		for (auto [dx,dy] : pat) { if (dx > max_dx) max_dx = dx; if (dy > max_dy) max_dy = dy; }
		std::vector<std::pair<size_t,size_t>> anchors;
		for (size_t sy = 0; sy + static_cast<size_t>(max_dy) < map.height; ++sy) {
			for (size_t sx = 0; sx + static_cast<size_t>(max_dx) < map.width; ++sx) {
				anchors.emplace_back(sx, sy);
			}
		}
		std::shuffle(anchors.begin(), anchors.end(), gen);
		for (auto [sx,sy] : anchors) {
			LabyrinthMap test = map;
			std::vector<std::pair<size_t,size_t>> cells;
			cells.reserve(pat.size());
			for (auto [dx,dy] : pat) cells.emplace_back(static_cast<size_t>(sx+dx), static_cast<size_t>(sy+dy));
			for (auto [x,y] : cells) test.set_cell(x, y, type);
			// Remove internal walls, close cluster perimeter
			std::vector<std::tuple<bool,size_t,size_t>> perimeter;
			for (auto [x,y] : cells) {
				// internal links
				if (x + 1 < test.width && keyIn(cells,x+1,y)) test.v_walls[y][x+1] = false;
				if (y + 1 < test.height && keyIn(cells,x,y+1)) test.h_walls[y+1][x] = false;
				// perimeter closing and candidates
				if (x==0 || !keyIn(cells,x-1,y)) { test.v_walls[y][x]=true; if (x>0) perimeter.emplace_back(true,y,x); }
				if (x+1==test.width || !keyIn(cells,x+1,y)) { test.v_walls[y][x+1]=true; if (x+1<test.width) perimeter.emplace_back(true,y,x+1); }
				if (y==0 || !keyIn(cells,x,y-1)) { test.h_walls[y][x]=true; if (y>0) perimeter.emplace_back(false,y,x); }
				if (y+1==test.height || !keyIn(cells,x,y+1)) { test.h_walls[y+1][x]=true; if (y+1<test.height) perimeter.emplace_back(false,y+1,x); }
			}
			if (perimeter.empty()) continue;
			std::shuffle(perimeter.begin(), perimeter.end(), gen);
			for (auto e : perimeter) {
				LabyrinthMap test2 = test;
				if (std::get<0>(e)) test2.v_walls[std::get<1>(e)][std::get<2>(e)] = false;
				else test2.h_walls[std::get<1>(e)][std::get<2>(e)] = false;
				if (count_components(test2) == 1) {
					map = std::move(test2);
					out_cells = std::move(cells);
					return true;
				}
			}
		}
	}
	return false;
}

} // namespace LocationUtils


