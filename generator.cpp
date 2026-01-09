#include "generator.hpp"
#include <random>
#include <stack>
#include <algorithm>
#include <unordered_set>
#include <queue>
#include "locations/Location.hpp"
#include "game.hpp"

static size_t count_components(const LabyrinthMap& m) {
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

static std::mt19937& rng() {
	static thread_local std::mt19937 g{std::random_device{}()};
	return g;
}

void set_rng_seed(unsigned int seed) {
	rng().seed(seed);
}

unsigned int rand_u32() {
	return rng()();
}

void carve_maze(LabyrinthMap& map) {
	std::vector<std::vector<bool>> visited(map.height, std::vector<bool>(map.width, false));
	std::stack<std::pair<size_t,size_t>> st;
	visited[0][0] = true;
	st.push({0,0});
	std::uniform_int_distribution<int> dist;
	while (!st.empty()) {
		auto [cx, cy] = st.top();
		std::vector<std::pair<size_t,size_t>> neighbors;
		if (cx > 0 && !visited[cy][cx-1]) neighbors.push_back({cx-1, cy});
		if (cx + 1 < map.width && !visited[cy][cx+1]) neighbors.push_back({cx+1, cy});
		if (cy > 0 && !visited[cy-1][cx]) neighbors.push_back({cx, cy-1});
		if (cy + 1 < map.height && !visited[cy+1][cx]) neighbors.push_back({cx, cy+1});
		if (neighbors.empty()) {
			st.pop();
			continue;
		}
		std::shuffle(neighbors.begin(), neighbors.end(), rng());
		auto [nx, ny] = neighbors.front();
		// remove wall between cells
		if (nx > cx) map.v_walls[cy][cx+1] = false;
		else if (nx < cx) map.v_walls[cy][cx] = false;
		else if (ny > cy) map.h_walls[cy+1][cx] = false;
		else if (ny < cy) map.h_walls[cy][cx] = false;
		visited[ny][nx] = true;
		st.push({nx, ny});
	}
}

void place_items(LabyrinthMap& map) {
	// Exit will be set on border edge separately
	std::vector<std::pair<size_t,size_t>> empties;
	for (size_t y = 0; y < map.height; ++y) {
		for (size_t x = 0; x < map.width; ++x) {
			if (map.get_cell(x, y) == CellContent::Empty) empties.emplace_back(x, y);
		}
	}
	std::shuffle(empties.begin(), empties.end(), rng());
	if (!empties.empty()) {
		auto [tx, ty] = empties.back(); empties.pop_back();
		map.set_cell(tx, ty, CellContent::Treasure);
	}
	// Hospital cluster will be placed separately
}

void remove_extra_walls(LabyrinthMap& map, float openness) {
	if (openness <= 0.0f) return;
	if (openness > 1.0f) openness = 1.0f;
	struct Edge { bool vertical; size_t y; size_t x; };
	std::vector<Edge> candidates;
	// internal vertical edges: x in [1..width-1]
	for (size_t y = 0; y < map.height; ++y) {
		for (size_t x = 1; x < map.width; ++x) {
			if (map.v_walls[y][x]) candidates.push_back({true, y, x});
		}
	}
	// internal horizontal edges: y in [1..height-1]
	for (size_t y = 1; y < map.height; ++y) {
		for (size_t x = 0; x < map.width; ++x) {
			if (map.h_walls[y][x]) candidates.push_back({false, y, x});
		}
	}
	std::shuffle(candidates.begin(), candidates.end(), rng());
	size_t remove_count = static_cast<size_t>(candidates.size() * openness);
	for (size_t i = 0; i < remove_count && i < candidates.size(); ++i) {
		auto e = candidates[i];
		if (e.vertical) {
			map.v_walls[e.y][e.x] = false;
		} else {
			map.h_walls[e.y][e.x] = false;
		}
	}
}

LabyrinthMap generate_maze_with_items(size_t width, size_t height, float openness) {
	LabyrinthMap map(width, height);
	carve_maze(map);
	remove_extra_walls(map, openness);
	place_exit_edge(map);
	ensure_exit_open(map);
	// Let locations place themselves (shape + walls) based on seed
	if (auto* hl = getLocationFor(CellContent::Hospital)) { Game d; hl->onPlaced(d, map); }
	if (auto* al = getLocationFor(CellContent::Arsenal)) { Game d; al->onPlaced(d, map); }
	// Place treasure after locations are placed
	place_items(map);
	return map;
}

void place_exit_edge(LabyrinthMap& map) {
	// choose a random border edge and open it; record as exit
	std::vector<std::tuple<bool,size_t,size_t>> candidates; // (vertical,y,x)
	// left border x=0 vertical edges
	for (size_t y = 0; y < map.height; ++y) candidates.emplace_back(true, y, 0);
	// right border x=width
	for (size_t y = 0; y < map.height; ++y) candidates.emplace_back(true, y, map.width);
	// top border y=0 horizontal
	for (size_t x = 0; x < map.width; ++x) candidates.emplace_back(false, 0, x);
	// bottom border y=height
	for (size_t x = 0; x < map.width; ++x) candidates.emplace_back(false, map.height, x);
	std::shuffle(candidates.begin(), candidates.end(), rng());
	for (auto& t : candidates) {
		bool vert = std::get<0>(t);
		size_t y = std::get<1>(t);
		size_t x = std::get<2>(t);
		// Prefer opening where adjacent cell exists (always true) and keep as exit
		map.has_exit = true;
		map.exit_vertical = vert;
		map.exit_y = y;
		map.exit_x = x;
		if (vert) {
			map.v_walls[y][x] = false;
		} else {
			map.h_walls[y][x] = false;
		}
		break;
	}
}

void ensure_exit_open(LabyrinthMap& map) {
	if (!map.has_exit) return;
	if (map.exit_vertical) {
		// vertical edge at (exit_x, exit_y)
		map.v_walls[map.exit_y][map.exit_x] = false;
	} else {
		// horizontal edge at (exit_x, exit_y)
		map.h_walls[map.exit_y][map.exit_x] = false;
	}
}

// Place arsenal cluster using same logic as hospital, with 'A' cells, single interior entrance and connectivity preserved
void place_arsenal_cluster(LabyrinthMap& map) {
	using P = std::vector<std::pair<int,int>>;
	std::vector<P> patterns = {
		{{0,0},{1,0},{0,1},{1,1}},
		{{0,0},{1,0},{2,0},{0,1},{1,1},{2,1}},
		{{0,0},{1,0},{0,1},{1,1},{0,2},{1,2}},
		{{0,0},{1,0},{2,0},{0,1},{1,1},{2,1},{0,2},{1,2},{2,2}},
		{{1,0},{0,1},{1,1},{2,1},{1,2}},
	};
	std::shuffle(patterns.begin(), patterns.end(), rng());
	std::vector<std::tuple<size_t,size_t,P>> candidates;
	for (const auto& pat : patterns) {
		int max_dx = 0, max_dy = 0;
		for (auto [dx,dy] : pat) { if (dx > max_dx) max_dx = dx; if (dy > max_dy) max_dy = dy; }
		for (size_t sy = 0; sy + static_cast<size_t>(max_dy) < map.height; ++sy) {
			for (size_t sx = 0; sx + static_cast<size_t>(max_dx) < map.width; ++sx) {
				candidates.emplace_back(sx, sy, pat);
			}
		}
	}
	if (candidates.empty()) return;
	std::shuffle(candidates.begin(), candidates.end(), rng());
	for (auto& cand : candidates) {
		size_t sx = std::get<0>(cand);
		size_t sy = std::get<1>(cand);
		const auto& pat = std::get<2>(cand);
		LabyrinthMap test = map;
		std::vector<std::pair<size_t,size_t>> cells;
		for (auto [dx,dy] : pat) cells.emplace_back(static_cast<size_t>(sx + dx), static_cast<size_t>(sy + dy));
		for (auto [x,y] : cells) test.set_cell(x, y, CellContent::Arsenal);
		std::unordered_set<long long> in;
		for (auto [x,y] : cells) in.insert(static_cast<long long>(y)*1000000LL + static_cast<long long>(x));
		for (auto [x,y] : cells) {
			long long nr = static_cast<long long>(y)*1000000LL + static_cast<long long>(x+1);
			if (x + 1 < test.width && in.count(nr)) test.v_walls[y][x+1] = false;
			long long nd = static_cast<long long>(y+1)*1000000LL + static_cast<long long>(x);
			if (y + 1 < test.height && in.count(nd)) test.h_walls[y+1][x] = false;
		}
		std::vector<std::tuple<bool,size_t,size_t>> perimeter;
		for (auto [x,y] : cells) {
			{ bool border = (x==0); long long n= (long long)y*1000000LL + (long long)(x-1);
			  if (border || !in.count(n)) { test.v_walls[y][x]=true; if(!border) perimeter.emplace_back(true,y,x); } }
			{ bool border = (x+1==test.width); long long n= (long long)y*1000000LL + (long long)(x+1);
			  if (border || !in.count(n)) { test.v_walls[y][x+1]=true; if(!border) perimeter.emplace_back(true,y,x+1); } }
			{ bool border = (y==0); long long n= (long long)(y-1)*1000000LL + (long long)x;
			  if (border || !in.count(n)) { test.h_walls[y][x]=true; if(!border) perimeter.emplace_back(false,y,x); } }
			{ bool border = (y+1==test.height); long long n= (long long)(y+1)*1000000LL + (long long)x;
			  if (border || !in.count(n)) { test.h_walls[y+1][x]=true; if(!border) perimeter.emplace_back(false,y+1,x); } }
		}
		if (perimeter.empty()) continue;
		std::shuffle(perimeter.begin(), perimeter.end(), rng());
		for (auto e : perimeter) {
			LabyrinthMap test2 = test;
			if (std::get<0>(e)) test2.v_walls[std::get<1>(e)][std::get<2>(e)] = false;
			else test2.h_walls[std::get<1>(e)][std::get<2>(e)] = false;
			if (count_components(test2) == 1) { map = std::move(test2); return; }
		}
	}
}

void place_hospital_cluster(LabyrinthMap& map) {
	// Patterns are sets of (dx,dy) offsets starting from anchor (sx,sy)
	using P = std::vector<std::pair<int,int>>;
	std::vector<P> patterns = {
		// 2x2
		{{0,0},{1,0},{0,1},{1,1}},
		// 3x2
		{{0,0},{1,0},{2,0},{0,1},{1,1},{2,1}},
		// 2x3
		{{0,0},{1,0},{0,1},{1,1},{0,2},{1,2}},
		// 3x3
		{{0,0},{1,0},{2,0},{0,1},{1,1},{2,1},{0,2},{1,2},{2,2}},
		// plus shape (center + arms)
		{{1,0},{0,1},{1,1},{2,1},{1,2}},
	};
	std::shuffle(patterns.begin(), patterns.end(), rng());

	// Find all valid anchors for any pattern (keep away from map border by at least 1)
	std::vector<std::tuple<size_t,size_t,P>> candidates;
	for (const auto& pat : patterns) {
		// Determine bounding box
		int max_dx = 0, max_dy = 0;
		for (auto [dx,dy] : pat) { if (dx > max_dx) max_dx = dx; if (dy > max_dy) max_dy = dy; }
		for (size_t sy = 0; sy + static_cast<size_t>(max_dy) < map.height; ++sy) {
			for (size_t sx = 0; sx + static_cast<size_t>(max_dx) < map.width; ++sx) {
				// Fits entirely within [0..width-1]/[0..height-1] (can touch border)
				candidates.emplace_back(sx, sy, pat);
			}
		}
	}
	if (candidates.empty()) return;
	std::shuffle(candidates.begin(), candidates.end(), rng());

	// Try candidates until placed without creating unreachable islands
	for (auto& cand : candidates) {
		size_t sx = std::get<0>(cand);
		size_t sy = std::get<1>(cand);
		const auto& pat = std::get<2>(cand);
		// Work on a copy to validate connectivity
		LabyrinthMap test = map;
		// Build set of cells
		std::vector<std::pair<size_t,size_t>> cells;
		cells.reserve(pat.size());
		for (auto [dx,dy] : pat) {
			cells.emplace_back(static_cast<size_t>(sx + dx), static_cast<size_t>(sy + dy));
		}
		// Mark all cells as Hospital
		for (auto [x,y] : cells) test.set_cell(x, y, CellContent::Hospital);
		// Remove internal walls between hospital cells
		std::unordered_set<long long> in;
		for (auto [x,y] : cells) in.insert(static_cast<long long>(y)*1000000LL + static_cast<long long>(x));
		for (auto [x,y] : cells) {
			long long nr = static_cast<long long>(y)*1000000LL + static_cast<long long>(x+1);
			if (x + 1 < test.width && in.count(nr)) test.v_walls[y][x+1] = false;
			long long nd = static_cast<long long>(y+1)*1000000LL + static_cast<long long>(x);
			if (y + 1 < test.height && in.count(nd)) test.h_walls[y+1][x] = false;
		}
		// Close perimeter around cluster
		std::vector<std::tuple<bool,size_t,size_t>> perimeter;
		for (auto [x,y] : cells) {
			// left edge (vertical at x)
			{
				bool is_border = (x == 0);
				long long nkey = static_cast<long long>(y)*1000000LL + static_cast<long long>(x-1);
				if (is_border || !in.count(nkey)) {
					test.v_walls[y][x] = true;
					if (!is_border) perimeter.emplace_back(true,y,x);
				}
			}
			// right edge (vertical at x+1)
			{
				bool is_border = (x + 1 == test.width);
				long long nkey = static_cast<long long>(y)*1000000LL + static_cast<long long>(x+1);
				if (is_border || !in.count(nkey)) {
					test.v_walls[y][x+1] = true;
					if (!is_border) perimeter.emplace_back(true,y,x+1);
				}
			}
			// top edge (horizontal at y)
			{
				bool is_border = (y == 0);
				long long nkey = static_cast<long long>(y-1)*1000000LL + static_cast<long long>(x);
				if (is_border || !in.count(nkey)) {
					test.h_walls[y][x] = true;
					if (!is_border) perimeter.emplace_back(false,y,x);
				}
			}
			// bottom edge (horizontal at y+1)
			{
				bool is_border = (y + 1 == test.height);
				long long nkey = static_cast<long long>(y+1)*1000000LL + static_cast<long long>(x);
				if (is_border || !in.count(nkey)) {
					test.h_walls[y+1][x] = true;
					if (!is_border) perimeter.emplace_back(false,y+1,x);
				}
			}
		}
		if (perimeter.empty()) continue;
		// Try entrances until connectivity preserved (single component)
		std::shuffle(perimeter.begin(), perimeter.end(), rng());
		for (auto e : perimeter) {
			LabyrinthMap test2 = test;
			if (std::get<0>(e)) test2.v_walls[std::get<1>(e)][std::get<2>(e)] = false;
			else test2.h_walls[std::get<1>(e)][std::get<2>(e)] = false;
			if (count_components(test2) == 1) {
				map = std::move(test2);
				return;
			}
		}
	}
}


