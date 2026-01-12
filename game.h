#pragma once

#include <cctype>
#include <fstream>

#include "context.h"
#include "maze.h"
#include "player.h"
#include "random.h"
#include "svg_helper.h"

struct TGameCfg {
    int32_t W;
    int32_t H;
    int32_t Openness;
    uint64_t Seed;
};

class TGame {
   public:
    TGame(const std::string& path) : Ctx(std::make_shared<TGameCtx>()) { FromFile(path); }
    TGame(const TGameCfg& cfg) : Ctx(std::make_shared<TGameCtx>(cfg.Seed)), Maze({cfg.W, cfg.H, cfg.Openness}, Ctx) {}

    void ToFile(const std::string& path) const {
        std::ofstream f(path);

        if (!f) {
            std::cerr << "can't open file " << path << std::endl;
            std::exit(1);
        }

        f << Ctx->Rng.GetMagic() << std::endl;
        Ctx->Rng.ToStream(f);

        f << Maze.GetMagic() << std::endl;
        Maze.ToStream(f);

        for (const auto& p : Players) {
            f << TPlayer::GetMagic() << std::endl;
            p.ToStream(f);
        }
    }

    void ToSvg(const std::string& path) const {
        std::ofstream f(path);

        if (!f) {
            std::cerr << "can't open file " << path << std::endl;
            std::exit(1);
        }

        f << GetSvg() << std::endl;
    }

    std::vector<std::string> AddPlayerRandom(const std::string& name) {
        TPos pos = Maze.GenRandomPosition();
        static const char* PALETTE[] = {"#1f77b4", "#ff7f0e", "#2ca02c", "#d62728", "#9467bd",
                                        "#8c564b", "#e377c2", "#7f7f7f", "#bcbd22", "#17becf"};
        std::string color = PALETTE[Players.size() % std::size(PALETTE)];
        Players.emplace_back(name, pos, std::move(color));
        std::vector<std::string> msgs;
        if (HasAdjacentAnother(Players.back())) {
            msgs.emplace_back(name + " почувствовал чье-то дыхание");
        }
        return msgs;
    }

    std::vector<std::string> MovePlayer(const std::string& name, EDir dir) {
        std::vector<std::string> msgs;
        auto it = std::find_if(Players.begin(), Players.end(), [&](const TPlayer& p) { return p.GetName() == name; });
        if (it == Players.end()) {
            std::cerr << "Player " << name << " not found" << std::endl;
            std::exit(1);
        }
        TPos cur = it->GetPos();
        TPos next = Maze.Step(cur, dir);
        if (!Maze.InBounds(next)) {
            msgs.emplace_back(it->GetName() + " врезался во внешнюю стену");
        } else if (Maze.At(cur).HasWall(dir)) {
            msgs.emplace_back(it->GetName() + " врезался в стену");
        } else {
            it->SetPos(next);
            msgs.emplace_back(it->GetName() + " прошел");
        }
        if (HasAdjacentAnother(*it)) {
            msgs.emplace_back(it->GetName() + " почувствовал чье-то дыхание");
        }
        return msgs;
    }

    std::vector<std::string> MovePlayer(const std::string& name, std::string_view dirStr) {
        EDir dir;
        if (!Maze.DirFromStr(dirStr, dir)) {
            std::cerr << "Wrong direction" << std::endl;
            std::exit(1);
        }
        return MovePlayer(name, dir);
    }

   private:
    bool IsValidMagic(const std::string& magic) const { return magic.front() == '[' && magic.back() == ']'; }

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

            if (magic == Ctx->Rng.GetMagic()) {
                Ctx->Rng.FromStream(f);
            } else if (magic == Maze.GetMagic()) {
                Maze.FromStream(f, Ctx);
            } else if (magic == TPlayer::GetMagic()) {
                Players.emplace_back().FromStream(f);
            }
        }
    }

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

        if (!Players.empty()) {
            out += SVG::GroupBegin("players");
            for (const auto& p : Players) {
                out += p.GetSvg(marginpx, cellpx);
            }
            out += SVG::GroupEnd();
        }

        out += SVG::GroupBegin("maze-border");
        out += SVG::DrawRect({marginpx, marginpx, marginpx + w * cellpx, marginpx + h * cellpx}, "none", "black",
                             std::format("stroke-width=\"{}\"", strokepx));
        out += SVG::GroupEnd();

        out += SVG::GetFooter();
        return out;
    }

   private:
    std::shared_ptr<TGameCtx> Ctx;
    TMaze Maze;
    std::vector<TPlayer> Players;

    bool HasAdjacentAnother(const TPlayer& who) const {
        for (const auto& p : Players) {
            if (&p == &who) continue;
            int dx = std::abs(p.GetPos().X - who.GetPos().X);
            int dy = std::abs(p.GetPos().Y - who.GetPos().Y);
            if (dx + dy == 1) return true;
        }
        return false;
    }
};
