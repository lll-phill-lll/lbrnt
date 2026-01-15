#pragma once

#include <format>
#include <string>
#include <string_view>
#include <vector>

#include "../svg_helper.h"
#include "base.h"

class TArsenal : public ILocation {
   public:
    std::string_view Type() const override { return "ARSENAL"; }

    std::vector<std::string> OnEnter(std::string_view player) const override {
        return {std::format("{} вошел в арсенал", player)};
    }

    std::vector<std::string> OnLeave(std::string_view player) const override {
        return {std::format("{} вышел из арсенала", player)};
    }

    std::string ToSvg(int marginpx, int cellpx) const override {
        std::string out;
        out += SVG::GroupBegin("arsenal", "fill=\"#ffcc00\" fill-opacity=\"0.25\" stroke=\"none\"");
        for (const auto& c : Cells) {
            out += SVG::DrawRect(SVG::ToRect(c, marginpx, cellpx), "#ffcc00");
        }
        out += SVG::GroupEnd();
        return out;
    }

    // std::size_t GetPlacementAttempts() const override { return 8; }

    // void BuildExtraOps(...) const override { ... }
};
