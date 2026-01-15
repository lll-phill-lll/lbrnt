#pragma once

#include <format>
#include <string>
#include <string_view>
#include <vector>

#include "../svg_helper.h"
#include "base.h"

class THospital : public ILocation {
   public:
    std::string_view Type() const override { return "HOSPITAL"; }

    std::vector<std::string> OnEnter(std::string_view player) const override {
        return {std::format("{} вошел в больницу", player)};
    }

    std::vector<std::string> OnLeave(std::string_view player) const override {
        return {std::format("{} вышел из больницы", player)};
    }

    std::string ToSvg(int marginpx, int cellpx) const override {
        std::string out;
        out += SVG::GroupBegin("hospital", "fill=\"#d62728\" fill-opacity=\"0.25\" stroke=\"none\"");
        for (const auto& c : Cells) {
            out += SVG::DrawRect(SVG::ToRect(c, marginpx, cellpx), "#d62728");
        }
        out += SVG::GroupEnd();
        return out;
    }
};
