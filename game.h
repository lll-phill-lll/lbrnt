#pragma once

#include <algorithm>
#include <cctype>
#include <fstream>
#include <memory>

#include "context.h"
#include "locations/base.h"
#include "locations/registry.h"
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
    TGame(const TGameCfg& cfg) : Ctx(std::make_shared<TGameCtx>(cfg.Seed)), Maze({cfg.W, cfg.H, cfg.Openness}, Ctx) {
        PlaceLocations();
    }

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
        for (const auto& loc : Locations) {
            f << ILocation::GetMagic() << std::endl;
            loc->ToStream(f);
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
        // TODO: maybe redundant
        if (HasAdjacentAnother(Players.back())) {
            msgs.emplace_back(name + " почувствовал чье-то дыхание");
        }

        for (const auto& loc : Locations) {
            if (loc->Contains(pos)) {
                auto ev = loc->OnEnter(name);
                msgs.insert(msgs.end(), ev.begin(), ev.end());
            }
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
            for (const auto& loc : Locations) {
                bool wasIn = loc->Contains(cur);
                bool willIn = loc->Contains(next);
                if (wasIn && !willIn) {
                    auto ev = loc->OnLeave(it->GetName());
                    msgs.insert(msgs.end(), ev.begin(), ev.end());
                }

                if (!wasIn && willIn) {
                    auto ev = loc->OnEnter(it->GetName());
                    msgs.insert(msgs.end(), ev.begin(), ev.end());
                }
            }
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

    void PlaceLocations() {
        std::vector<TPos> reserved;

        for (const auto& [type, _] : GetLocationRegistry().All()) {
            for (;;) {
                auto loc = GetLocationRegistry().Create(type);
                std::vector<std::pair<TPos, EDir>> toPlace, toClear;
                if (!loc->Propose(Maze, Ctx->Rng, reserved, toPlace, toClear)) continue;
                if (Maze.TryApplyLocation(loc->GetCells(), toPlace, toClear, reserved)) {
                    for (auto c : loc->GetCells()) reserved.push_back(c);
                    Locations.push_back(std::move(loc));
                    break;
                }
            }
        }
    }

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
            } else if (magic == ILocation::GetMagic()) {
                Locations.push_back(GetLocationRegistry().ReadOne(f));
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

        out += SVG::GroupBegin("grid", "stroke=\"#dddddd\" stroke-width=\"1\" fill=\"none\"");
        for (int x = 0; x <= w; ++x) {
            int gx = marginpx + x * cellpx;
            out += SVG::DrawLine(gx, marginpx, gx, marginpx + h * cellpx);
        }
        for (int y = 0; y <= h; ++y) {
            int gy = marginpx + y * cellpx;
            out += SVG::DrawLine(marginpx, gy, marginpx + w * cellpx, gy);
        }
        out += SVG::GroupEnd();

        // Locations first
        if (!Locations.empty()) {
            out += SVG::GroupBegin("locations");
            for (const auto& loc : Locations) out += loc->ToSvg(marginpx, cellpx);
            out += SVG::GroupEnd();
        }

        // Cell labels
        out += SVG::GroupBegin("coords", "opacity=\"0.08\"");
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                SVG::TRect rect = SVG::ToRect({x, y}, marginpx, cellpx);
                out += SVG::DrawText(rect.GetCX(), rect.GetCY(), std::format("{},{}", x, y), "#000000", 6.0f);
            }
        }
        out += SVG::GroupEnd();

        // Walls after everything
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
            std::vector<const TPlayer*> sorted;
            sorted.reserve(Players.size());
            for (const auto& p : Players) sorted.push_back(&p);
            std::sort(sorted.begin(), sorted.end(), [](const TPlayer* a, const TPlayer* b) {
                if (a->GetPos().Y != b->GetPos().Y) return a->GetPos().Y < b->GetPos().Y;
                if (a->GetPos().X != b->GetPos().X) return a->GetPos().X < b->GetPos().X;
                return a->GetName() < b->GetName();
            });
            for (size_t i = 0; i < sorted.size();) {
                size_t j = i + 1;
                const TPos pos = sorted[i]->GetPos();
                while (j < sorted.size() && sorted[j]->GetPos().X == pos.X && sorted[j]->GetPos().Y == pos.Y) {
                    ++j;
                }
                int32_t count = j - i;
                for (int32_t k = 0; k < count; ++k) {
                    out += sorted[i + k]->GetSvgInCell(marginpx, cellpx, count, k);
                }
                i = j;
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
    std::vector<std::unique_ptr<ILocation>> Locations;

    bool HasAdjacentAnother(const TPlayer& who) const {
        static const EDir kDirs[4] = {EDir::UP, EDir::RIGHT, EDir::DOWN, EDir::LEFT};
        const TPos pos = who.GetPos();
        for (EDir dir : kDirs) {
            if (Maze.At(pos).HasWall(dir)) continue;
            TPos neigh = Maze.Step(pos, dir);
            if (!Maze.InBounds(neigh)) continue;
            for (const auto& p : Players) {
                if (&p == &who) continue;
                if (p.GetPos() == neigh) {
                    return true;
                }
            }
        }
        return false;
    }
};
