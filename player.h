#pragma once

#include <format>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "common.h"
#include "svg_helper.h"

class TPlayer {
   public:
    TPlayer() = default;
    TPlayer(std::string name, TPos pos, std::string color) : Name(std::move(name)), Pos(pos), Color(std::move(color)) {}

    void ToStream(std::ostream& out) const { out << std::format("{} {} {} {}\n", Name, Pos.X, Pos.Y, Color); }

    static std::string_view GetMagic() { return Magic; }

    void FromStream(std::istream& in) {
        if (!(in >> Name >> Pos.X >> Pos.Y >> Color)) {
            std::cerr << "Failed to read player fields (name x y color)" << std::endl;
            std::exit(1);
        }
    }

    std::string GetSvg(int marginpx, int cellpx) const {
        std::string out;
        SVG::TRect rect = SVG::ToRect(Pos, marginpx, cellpx);
        int cx = rect.GetCX();
        int cy = rect.GetCY();
        int r = cellpx / 3;
        out += SVG::svgCircle(cx, cy, r, Color, "black");
        char ch = Name.empty() ? '?' : static_cast<char>(std::toupper(static_cast<unsigned char>(Name[0])));
        out += SVG::DrawText(cx, cy, std::string(1, ch), "white", static_cast<float>(cellpx * 0.5f), "font-weight=\"bold\"");
        return out;
    }

    std::string GetSvgInCell(int marginpx, int cellpx, int totalInCell, int indexInCell) const {
        if (totalInCell <= 1) return GetSvg(marginpx, cellpx);
        std::string out;
        SVG::TRect rect = SVG::ToRect(Pos, marginpx, cellpx);
        int cols = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(totalInCell))));
        int rows = (totalInCell + cols - 1) / cols;
        int gridW = rect.GetW() / cols;
        int gridH = rect.GetH() / rows;
        int row = indexInCell / cols;
        int col = indexInCell % cols;
        int cx = rect.x0 + col * gridW + gridW / 2;
        int cy = rect.y0 + row * gridH + gridH / 2;
        int r = std::max(2, std::min(gridW, gridH) / 2 - 2);
        float fontSize = std::max(3.0f, r * 0.9f);
        out += SVG::svgCircle(cx, cy, r, Color, "black");
        char ch = Name.empty() ? '?' : static_cast<char>(std::toupper(static_cast<unsigned char>(Name[0])));
        out += SVG::DrawText(cx, cy, std::string(1, ch), "white", fontSize, "font-weight=\"bold\"");
        return out;
    }

    const std::string& GetName() const { return Name; }
    const std::string& GetColor() const { return Color; }
    TPos GetPos() const { return Pos; }
    void SetPos(TPos p) { Pos = p; }

   private:
    std::string Name;
    TPos Pos{0, 0};
    std::vector<std::string> Inventory;
    std::string Color;

    static constexpr std::string_view Magic = "[PLAYER]";
};
