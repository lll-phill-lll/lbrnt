#pragma once

#include <fstream>

#include "maze.h"
#include "svg_helper.h"

struct TGameCfg {
    int32_t W;
    int32_t H;
    int32_t Openness;
};

class TGame {
   public:
    TGame(const std::string& path) { FromFile(path); }
    TGame(const TGameCfg& cfg) : Maze({cfg.W, cfg.H, cfg.Openness}) {}

    void FromFile(const std::string& path) {
        std::ifstream f(path);
        if (!f) {
            std::cerr << "can't open file " << path << std::endl;
            std::exit(1);
        }

        std::string magic;
        while (f >> magic) {
            if (!IsValidMagic(magic)) {
                std::cerr << "can't parse magic " << magic << std::endl;
                std::exit(1);
            }

            if (magic == Maze.GetMagic()) {
                Maze.FromStream(f);
            }
        }
    }

    void ToFile(const std::string& path) const {
        std::ofstream f(path);

        if (!f) {
            std::cerr << "can't open file " << path << std::endl;
            std::exit(1);
        }

        f << Maze.GetMagic() << std::endl;
        Maze.ToStream(f);
    }

    void ToSvg(const std::string& path) const {
        std::ofstream f(path);

        if (!f) {
            std::cerr << "can't open file " << path << std::endl;
            std::exit(1);
        }

        f << GetSvg() << std::endl;
    }

   private:
    bool IsValidMagic(const std::string& magic) const { return magic.front() == '[' && magic.back() == ']'; }

    std::string GetSvg() const {
        constexpr int32_t marginpx = 10;
        constexpr int32_t cellpx = 24;
        constexpr int32_t strokepx = 2;

        int32_t h = Maze.GetH();
        int32_t w = Maze.GetW();

        const int wpx = marginpx * 2 + w * cellpx;
        const int hpx = marginpx * 2 + h * cellpx;

        std::string out;
        out.reserve(size_t(wpx) * 2);

        out += SVG::GetHeader(wpx, hpx);

        out += SVG::GroupBegin();
        out += SVG::DrawRect({0, 0, wpx, hpx}, "white");
        out += SVG::GroupEnd();

        out += SVG::GroupBegin("walls", std::format("stroke=\"black\" stroke-width=\"{}\" fill=\"none\"", strokepx));
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                TPos pos{x, y};
                SVG::TRect rect = SVG::ToRect(pos, marginpx, cellpx);

                if (Maze.At(pos).HasWall(EDir::UP)) {
                    out += SVG::DrawLine(rect.x0, rect.y0, rect.x1, rect.y0);
                }

                if (Maze.At(pos).HasWall(EDir::LEFT)) {
                    out += SVG::DrawLine(rect.x0, rect.y0, rect.x0, rect.y1);
                }
            }
        }
        out += SVG::GroupEnd();

        out += SVG::GroupBegin("maze-border");
        out += SVG::DrawRect({marginpx, marginpx, marginpx + w * cellpx, marginpx + h * cellpx}, "none", "black",
                             std::format("stroke-width=\"{}\"", strokepx));
        out += SVG::GroupEnd();

        out += SVG::GetFooter();
        return out;
    }

   private:
    TMaze Maze;
};
