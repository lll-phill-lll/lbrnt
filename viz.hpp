#pragma once
#include "state.hpp"
#include <string>

std::string render_svg(const AppState& st, float cell_px, float margin_px);
std::string render_html(const AppState& st, float cell_px, float margin_px);


