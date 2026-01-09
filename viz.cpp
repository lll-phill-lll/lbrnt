#include "viz.hpp"
#include <sstream>
#include <iomanip>
#include <unordered_map>
#include <vector>
#include <map>
#include <algorithm>

std::string render_svg(const AppState& st, float cell_px, float margin_px) {
	const auto& map = st.map;
	float map_w = static_cast<float>(map.width) * cell_px;
	float map_h = static_cast<float>(map.height) * cell_px;
	float panel_w = std::max(cell_px * 7.0f, 160.0f);
	float panel_gap = margin_px;
	float w = margin_px * 2.0f + map_w + panel_gap + panel_w;
	float h = margin_px * 2.0f + map_h;
	float sw = std::max(cell_px, 8.0f) / 12.0f;
	std::ostringstream oss;
	oss << R"(<svg xmlns="http://www.w3.org/2000/svg")"
	    << " viewBox=\"0 0 " << w << " " << h << "\""
	    << " width=\"" << w << "\" height=\"" << h << "\">";
	oss << "<rect x=\"0\" y=\"0\" width=\"" << w << "\" height=\"" << h << "\" fill=\"#ffffff\"/>\n";
	// grid faint
	for (size_t y = 0; y < map.height; ++y) {
		for (size_t x = 0; x < map.width; ++x) {
			oss << "<rect x=\"" << (margin_px + x * cell_px) << "\" y=\"" << (margin_px + y * cell_px)
			    << "\" width=\"" << cell_px << "\" height=\"" << cell_px
			    << "\" fill=\"#ffffff\" stroke=\"#f0f0f0\" stroke-width=\"1\"/>\n";
		}
	}
	// walls
	for (size_t y = 0; y < map.height; ++y) {
		for (size_t x = 0; x <= map.width; ++x) {
			if (map.v_walls[y][x]) {
				float xp = margin_px + x * cell_px;
				float y1 = margin_px + y * cell_px;
				float y2 = margin_px + (y + 1) * cell_px;
				oss << "<line x1=\"" << xp << "\" y1=\"" << y1 << "\" x2=\"" << xp << "\" y2=\"" << y2
				    << "\" stroke=\"#000\" stroke-width=\"" << sw << "\" stroke-linecap=\"square\"/>\n";
			} else if (map.has_exit && map.exit_vertical && map.exit_y == y && map.exit_x == x) {
				// draw exit mark (short green tick)
				float xp = margin_px + x * cell_px;
				float cy = margin_px + y * cell_px + cell_px * 0.5f;
				float len = cell_px * 0.3f;
				oss << "<line x1=\"" << xp << "\" y1=\"" << (cy - len*0.5f) << "\" x2=\"" << xp << "\" y2=\"" << (cy + len*0.5f)
				    << "\" stroke=\"#2e7d32\" stroke-width=\"" << (sw*1.2f) << "\" stroke-linecap=\"round\"/>\n";
			}
		}
	}
	for (size_t y = 0; y <= map.height; ++y) {
		for (size_t x = 0; x < map.width; ++x) {
			if (map.h_walls[y][x]) {
				float yp = margin_px + y * cell_px;
				float x1 = margin_px + x * cell_px;
				float x2 = margin_px + (x + 1) * cell_px;
				oss << "<line x1=\"" << x1 << "\" y1=\"" << yp << "\" x2=\"" << x2 << "\" y2=\"" << yp
				    << "\" stroke=\"#000\" stroke-width=\"" << sw << "\" stroke-linecap=\"square\"/>\n";
			}
		}
	}
	// Exit overlay (always draw mark regardless of wall presence)
	if (map.has_exit) {
		if (map.exit_vertical) {
			float xp = margin_px + map.exit_x * cell_px;
			float cy = margin_px + map.exit_y * cell_px + cell_px * 0.5f;
			float len = cell_px * 0.36f;
			oss << "<line x1=\"" << xp << "\" y1=\"" << (cy - len*0.5f) << "\" x2=\"" << xp << "\" y2=\"" << (cy + len*0.5f)
			    << "\" stroke=\"#2e7d32\" stroke-width=\"" << (sw*1.4f) << "\" stroke-linecap=\"round\"/>\n";
		} else {
			float yp = margin_px + map.exit_y * cell_px;
			float cx = margin_px + map.exit_x * cell_px + cell_px * 0.5f;
			float len = cell_px * 0.36f;
			oss << "<line x1=\"" << (cx - len*0.5f) << "\" y1=\"" << yp << "\" x2=\"" << (cx + len*0.5f) << "\" y2=\"" << yp
			    << "\" stroke=\"#2e7d32\" stroke-width=\"" << (sw*1.4f) << "\" stroke-linecap=\"round\"/>\n";
		}
	}
	// items
	for (size_t y = 0; y < map.height; ++y) {
		for (size_t x = 0; x < map.width; ++x) {
			float cx = margin_px + x * cell_px + cell_px * 0.5f;
			float cy = margin_px + y * cell_px + cell_px * 0.5f;
			switch (map.get_cell(x, y)) {
				case CellContent::Empty: break;
				case CellContent::Treasure:
					oss << "<circle cx=\"" << cx << "\" cy=\"" << cy << "\" r=\"" << (cell_px*0.25f)
					    << "\" fill=\"#d4af37\" stroke=\"#8b7d2b\" stroke-width=\"" << (sw*0.5f) << "\"/>\n";
					break;
				case CellContent::Hospital:
					oss << "<rect x=\"" << (margin_px + x*cell_px) << "\" y=\"" << (margin_px + y*cell_px) << "\" width=\"" << cell_px
					    << "\" height=\"" << cell_px << "\" fill=\"#d32f2f\" fill-opacity=\"0.7\"/>\n";
					break;
				case CellContent::Arsenal:
					oss << "<rect x=\"" << (margin_px + x*cell_px) << "\" y=\"" << (margin_px + y*cell_px) << "\" width=\"" << cell_px
					    << "\" height=\"" << cell_px << "\" fill=\"#ffd54f\" fill-opacity=\"0.7\"/>\n";
					break;
				case CellContent::Exit:
					oss << "<rect x=\"" << (margin_px + x*cell_px+sw) << "\" y=\"" << (margin_px + y*cell_px+sw) << "\" width=\"" << (cell_px-2*sw)
					    << "\" height=\"" << (cell_px-2*sw) << "\" fill=\"none\" stroke=\"#2e7d32\" stroke-width=\"" << sw << "\"/>\n";
					break;
			}
			// overlay ground loot (treasure) always if present
			long long key = (long long)y * 1000000LL + (long long)x;
			auto itL = st.game.loot_treasure.find(key);
			if (itL != st.game.loot_treasure.end() && itL->second > 0) {
				float lx = margin_px + x * cell_px + cell_px * 0.78f;
				float ly = margin_px + y * cell_px + cell_px * 0.78f;
				float rr = cell_px * 0.12f;
				oss << "<circle cx=\"" << lx << "\" cy=\"" << ly << "\" r=\"" << rr
				    << "\" fill=\"#d4af37\" stroke=\"#8b7d2b\" stroke-width=\"" << (sw*0.4f) << "\"/>\n";
				if (itL->second > 1) {
					oss << "<text x=\"" << lx << "\" y=\"" << (ly + cell_px*0.01f) << "\" fill=\"#5d4300\" font-size=\""
					    << (cell_px*0.3f) << "\" font-family=\"monospace\" text-anchor=\"middle\" dominant-baseline=\"central\">"
					    << itL->second << "</text>\n";
				}
			}
			// overlay ground items (centered letters)
			auto itGI = st.game.ground_items.find(key);
			if (itGI != st.game.ground_items.end() && !itGI->second.empty()) {
				bool hasK = itGI->second.count("knife") > 0;
				bool hasF = itGI->second.count("flashlight") > 0;
				bool hasR = itGI->second.count("rifle") > 0;
				bool hasS = itGI->second.count("shotgun") > 0;
				int cntK = hasK ? itGI->second.at("knife") : 0;
				int cntF = hasF ? itGI->second.at("flashlight") : 0;
				int cntR = hasR ? itGI->second.at("rifle") : 0;
				int cntS = hasS ? itGI->second.at("shotgun") : 0;
				int distinct = (hasK?1:0) + (hasF?1:0) + (hasR?1:0) + (hasS?1:0);
				std::string label;
				if (hasK) label.push_back('K');
				if (hasF) label.push_back('F');
				if (hasR) label.push_back('R');
				if (hasS) label.push_back('S');
				// If only single item present and count > 1, append count to label
				if (distinct == 1) {
					int c = hasK?cntK : hasF?cntF : hasR?cntR : cntS;
					if (c > 1) label += std::to_string(c);
				}
				oss << "<text x=\"" << cx << "\" y=\"" << cy << "\" fill=\"#111\" font-size=\""
				    << (cell_px*0.55f) << "\" font-family=\"monospace\" text-anchor=\"middle\" dominant-baseline=\"central\">"
				    << label << "</text>\n";
			}
		}
	}
	// players (support multiple per cell - draw smaller circles in cluster)
	{
		// Build turn order index for stable ordering when enforced
		std::unordered_map<std::string, size_t> orderIndex;
		if (st.game.enforce_turns && !st.game.turn_order.empty()) {
			for (size_t i = 0; i < st.game.turn_order.size(); ++i) orderIndex[st.game.turn_order[i]] = i;
		}
		std::map<long long, std::vector<std::pair<std::string,std::string>>> groups;
		for (const auto& kv : st.game.players) {
			long long key = (long long)kv.second.second * 1000000LL + (long long)kv.second.first;
			std::string col = "#1f77b4";
			auto itc = st.game.player_color.find(kv.first);
			if (itc != st.game.player_color.end()) col = itc->second;
			groups[key].push_back({kv.first, col});
		}
		std::string current_actor;
		if (st.game.enforce_turns && !st.game.turn_order.empty() && st.game.turn_index < st.game.turn_order.size()) {
			current_actor = st.game.turn_order[st.game.turn_index];
		}
		for (const auto& g : groups) {
			size_t gy = (size_t)(g.first / 1000000LL);
			size_t gx = (size_t)(g.first % 1000000LL);
			float base_x = margin_px + gx * cell_px + cell_px * 0.5f;
			float base_y = margin_px + gy * cell_px + cell_px * 0.5f;
			auto vec = g.second;
			std::sort(vec.begin(), vec.end(), [&](const auto& a, const auto& b){
				auto ia = orderIndex.find(a.first);
				auto ib = orderIndex.find(b.first);
				if (ia != orderIndex.end() && ib != orderIndex.end()) return ia->second < ib->second;
				if (ia != orderIndex.end() && ib == orderIndex.end()) return true;
				if (ia == orderIndex.end() && ib != orderIndex.end()) return false;
				return a.first < b.first;
			});
			size_t n = vec.size();
			// offsets and sizes
			std::vector<std::pair<float,float>> offs;
			float rr = (n <= 1 ? cell_px*0.28f : cell_px*0.20f);
			float txdy = (n <= 1 ? cell_px*0.02f : 0.0f);
			if (n == 1) {
				offs = {{0.0f, 0.0f}};
			} else if (n == 2) {
				float d = cell_px * 0.18f;
				offs = {{-d, 0.0f}, {d, 0.0f}};
			} else if (n == 3) {
				float dx = cell_px * 0.16f, dy = cell_px * 0.14f;
				offs = {{0.0f, -dy}, {-dx, dy}, {dx, dy}};
			} else {
				float dx = cell_px * 0.18f, dy = cell_px * 0.18f;
				offs = {{-dx,-dy}, {dx,-dy}, {-dx,dy}, {dx,dy}};
			}
			for (size_t i = 0; i < vec.size() && i < offs.size(); ++i) {
				float cx = base_x + offs[i].first;
				float cy = base_y + offs[i].second;
				const std::string& name = vec[i].first;
				const std::string& col = vec[i].second;
				oss << "<circle cx=\"" << cx << "\" cy=\"" << cy << "\" r=\"" << rr
				    << "\" fill=\"" << col << "\" opacity=\"0.9\"/>\n";
				// highlight current actor
				if (!current_actor.empty() && name == current_actor) {
					oss << "<circle cx=\"" << cx << "\" cy=\"" << cy << "\" r=\"" << (rr + sw*0.8f)
					    << "\" fill=\"none\" stroke=\"#ff9800\" stroke-width=\"" << (sw*1.6f) << "\"/>\n";
				}
				char label = name.empty() ? 'P' : static_cast<char>(::toupper(name[0]));
				oss << "<text x=\"" << cx << "\" y=\"" << (cy + txdy) << "\" fill=\"#ffffff\" font-size=\""
				    << (n <= 1 ? cell_px*0.45f : cell_px*0.34f) << "\" font-family=\"monospace\" text-anchor=\"middle\" dominant-baseline=\"central\">"
				    << label << "</text>\n";
			}
		}
	}
	// Stats panel on the right
	float panel_left = margin_px + map_w + panel_gap;
	float panel_top = margin_px;
	float panel_bottom = h - margin_px;
	oss << "<rect x=\"" << panel_left << "\" y=\"" << panel_top << "\" width=\"" << panel_w << "\" height=\"" << (panel_bottom - panel_top)
	    << "\" fill=\"#ffffff\" stroke=\"#cccccc\" stroke-width=\"" << (sw*0.6f) << "\"/>\n";
	oss << "<text x=\"" << (panel_left + panel_w*0.5f) << "\" y=\"" << (panel_top + cell_px*0.6f) << "\" fill=\"#111\" font-size=\""
	    << (cell_px*0.5f) << "\" font-family=\"monospace\" text-anchor=\"middle\" dominant-baseline=\"central\">Players</text>\n";
	float row_h = cell_px * 1.2f;
	float ycur = panel_top + cell_px * 1.4f;
	// stable order by player name
	std::string current_actor_panel;
	if (st.game.enforce_turns && !st.game.turn_order.empty() && st.game.turn_index < st.game.turn_order.size()) {
		current_actor_panel = st.game.turn_order[st.game.turn_index];
	}
	std::vector<std::string> names;
	if (st.game.enforce_turns && !st.game.turn_order.empty()) {
		// Use fixed turn order for panel; filter to currently present players
		for (const auto& n : st.game.turn_order) {
			if (st.game.players.count(n)) names.push_back(n);
		}
		// Append any stragglers (unlikely), alphabetically
		for (const auto& kv : st.game.players) {
			if (std::find(names.begin(), names.end(), kv.first) == names.end()) names.push_back(kv.first);
		}
	} else {
		names.reserve(st.game.players.size());
		for (const auto& kv : st.game.players) names.push_back(kv.first);
		std::sort(names.begin(), names.end());
	}
	for (const auto& name : names) {
		auto itp = st.game.players.find(name);
		if (itp == st.game.players.end()) continue;
		std::string col = "#1f77b4";
		auto itc = st.game.player_color.find(name);
		if (itc != st.game.player_color.end()) col = itc->second;
		float cx = panel_left + cell_px * 0.5f;
		float cy = ycur + row_h * 0.5f;
		oss << "<circle cx=\"" << cx << "\" cy=\"" << cy << "\" r=\"" << (cell_px*0.35f)
		    << "\" fill=\"" << col << "\" opacity=\"0.9\"/>\n";
		if (!current_actor_panel.empty() && name == current_actor_panel) {
			oss << "<circle cx=\"" << cx << "\" cy=\"" << cy << "\" r=\"" << (cell_px*0.35f + sw*0.8f)
			    << "\" fill=\"none\" stroke=\"#ff9800\" stroke-width=\"" << (sw*1.6f) << "\"/>\n";
		}
		char label = name.empty() ? 'P' : static_cast<char>(::toupper(name[0]));
		oss << "<text x=\"" << cx << "\" y=\"" << (cy + cell_px*0.02f) << "\" fill=\"#ffffff\" font-size=\""
		    << (cell_px*0.5f) << "\" font-family=\"monospace\" text-anchor=\"middle\" dominant-baseline=\"central\">"
		    << label << "</text>\n";
		oss << "<text x=\"" << (panel_left + cell_px*1.2f) << "\" y=\"" << (cy + cell_px*0.02f) << "\" fill=\"#111\" font-size=\""
		    << (cell_px*0.5f) << "\" font-family=\"monospace\" dominant-baseline=\"central\">" << name << "</text>\n";
		// items: knife status, treasure
		bool broken = st.game.broken_knife.count(name) > 0;
		std::string kcol = broken ? "#d32f2f" : "#2e7d32";
		// layout anchors
		float fx = panel_left + panel_w - cell_px * 3.8f;
		float rx = panel_left + panel_w - cell_px * 3.0f;
		float sx = panel_left + panel_w - cell_px * 2.6f;
		float kx = panel_left + panel_w - cell_px * 2.2f;
		std::string icol = "#333333";
		// show items the player has; grey out when charges==0
		auto itInv = st.game.inventories.find(name);
		auto get_ch = [&](const char* id)->int {
			if (itInv == st.game.inventories.end()) return 0;
			return itInv->second.getCharges(id);
		};
		auto draw_item = [&](float x, const char* label, bool present, bool active, const std::string& active_col) {
			if (!present) return;
			const std::string coltxt = active ? active_col : std::string("#999999");
			oss << "<text x=\"" << x << "\" y=\"" << (cy + cell_px*0.02f) << "\" fill=\"" << coltxt
			    << "\" font-size=\"" << (cell_px*0.6f) << "\" font-family=\"monospace\" text-anchor=\"middle\" dominant-baseline=\"central\">"
			    << label << "</text>\n";
		};
		bool hasFlash = itInv != st.game.inventories.end() && itInv->second.item_charges.count("flashlight");
		bool hasRifle  = itInv != st.game.inventories.end() && itInv->second.item_charges.count("rifle");
		bool hasShot   = itInv != st.game.inventories.end() && itInv->second.item_charges.count("shotgun");
		bool hasKnife  = itInv != st.game.inventories.end() && itInv->second.item_charges.count("knife");
		draw_item(fx, "F", hasFlash, get_ch("flashlight") > 0, icol);
		draw_item(rx, "R", hasRifle,  get_ch("rifle") > 0,      icol);
		draw_item(sx, "S", hasShot,   get_ch("shotgun") > 0,    icol);
		draw_item(kx, "K", true,      get_ch("knife") > 0 && !broken, kcol);
		bool hasT = st.game.players_with_treasure.count(name) > 0;
		if (hasT) {
			float tx = panel_left + panel_w - cell_px * 0.9f;
			oss << "<circle cx=\"" << tx << "\" cy=\"" << cy << "\" r=\"" << (cell_px*0.22f)
			    << "\" fill=\"#d4af37\" stroke=\"#8b7d2b\" stroke-width=\"" << (sw*0.4f) << "\"/>\n";
		}
		ycur += row_h;
	}
	oss << "</svg>\n";
	return oss.str();
}


