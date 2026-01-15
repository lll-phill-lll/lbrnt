#pragma once

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <istream>
#include <ostream>
#include <random>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "../common.h"
#include "../maze.h"
#include "../random.h"
#include "default_shapes.h"

class ILocation {
   public:
    virtual ~ILocation() = default;

    virtual std::string_view Type() const = 0;
    virtual std::vector<std::string> OnEnter(std::string_view player) const = 0;
    virtual std::vector<std::string> OnLeave(std::string_view player) const = 0;
    virtual std::string ToSvg(int marginpx, int cellpx) const = 0;

    bool Propose(TMaze& maze, TRng& rng, const std::vector<TPos>& reserved,
                 std::vector<std::pair<TPos, EDir>>& wallsToPlace, std::vector<std::pair<TPos, EDir>>& wallsToClear) {
        const std::size_t attempts = GetPlacementAttempts();

        std::uniform_int_distribution<int> px(0, maze.GetW() - 1);
        std::uniform_int_distribution<int> py(0, maze.GetH() - 1);

        for (std::size_t it = 0; it < attempts; ++it) {
            const auto& offs = PickShape(rng);

            const TPos a = PickAnchor(maze, rng, px, py);

            if (!TryPlaceCells(maze, reserved, offs, a)) {
                continue;
            }

            BuildOpenInteriorOps(maze, wallsToClear);
            BuildEncloseWithSingleExitOps(maze, rng, wallsToPlace, wallsToClear);

            BuildExtraOps(maze, rng, wallsToPlace, wallsToClear);

            return true;
        }
        return false;
    }

    virtual void ToStream(std::ostream& out) const {
        out << Type() << "\n";
        out << Cells.size() << "\n";
        for (const auto& p : Cells) {
            out << p.X << ' ' << p.Y << "\n";
        }
    }

    virtual void FromStream(std::istream& in) {
        std::size_t n = 0;
        if (!(in >> n)) {
            std::cerr << "bad location cells count\n";
            std::exit(1);
        }
        Cells.clear();
        Cells.reserve(n);

        for (std::size_t i = 0; i < n; ++i) {
            int32_t x = 0, y = 0;
            if (!(in >> x >> y)) {
                std::cerr << "bad location cell\n";
                std::exit(1);
            }
            Cells.push_back({x, y});
        }
    }

    static std::string_view GetMagic() { return Magic; }

    const std::vector<TPos>& GetCells() const { return Cells; }

    bool Contains(TPos p) const {
        for (const auto& c : Cells)
            if (c == p) return true;
        return false;
    }

   protected:
    virtual std::size_t GetPlacementAttempts() const { return 1; }

    virtual const std::vector<TPos>& PickShape(TRng& rng) const { return PickRandomShape(rng); }

    virtual TPos PickAnchor(const TMaze& /*maze*/, TRng& rng, std::uniform_int_distribution<int>& px,
                            std::uniform_int_distribution<int>& py) const {
        return {px(rng), py(rng)};
    }

    virtual void BuildExtraOps(TMaze& /*maze*/, TRng& /*rng*/, std::vector<std::pair<TPos, EDir>>& /*wallsToPlace*/,
                               std::vector<std::pair<TPos, EDir>>& /*wallsToClear*/) const {}

   protected:
    bool TryPlaceCells(const TMaze& maze, const std::vector<TPos>& reserved, const std::vector<TPos>& offs, TPos a) {
        Cells.clear();
        Cells.reserve(offs.size());

        for (const auto& o : offs) {
            TPos p{a.X + o.X, a.Y + o.Y};
            if (!maze.InBounds(p)) {
                Cells.clear();
                return false;
            }
            Cells.push_back(p);
        }

        if (IntersectsReserved(reserved)) {
            Cells.clear();
            return false;
        }
        return true;
    }

    void BuildEncloseWithSingleExitOps(TMaze& maze, TRng& rng, std::vector<std::pair<TPos, EDir>>& wallsToPlace,
                                       std::vector<std::pair<TPos, EDir>>& wallsToClear) const {
        std::vector<std::pair<TPos, EDir>> borders;
        borders.reserve(Cells.size() * 4);

        for (const auto& p : Cells) {
            for (EDir d : {EDir::UP, EDir::RIGHT, EDir::DOWN, EDir::LEFT}) {
                const TPos q = maze.Step(p, d);

                bool qInside = false;
                for (const auto& c : Cells) {
                    if (c == q) {
                        qInside = true;
                        break;
                    }
                }

                if (!qInside) {
                    if (maze.InBounds(q)) {
                        wallsToPlace.emplace_back(p, d);
                        borders.emplace_back(p, d);
                    }
                } else {
                    wallsToClear.emplace_back(p, d);
                }
            }
        }

        if (!borders.empty()) {
            std::uniform_int_distribution<std::size_t> pick(0, borders.size() - 1);
            const auto [p, d] = borders[pick(rng)];
            wallsToClear.emplace_back(p, d);
        }
    }

    void BuildOpenInteriorOps(TMaze& maze, std::vector<std::pair<TPos, EDir>>& wallsToClear) const {
        for (const auto& p : Cells) {
            for (EDir d : {EDir::UP, EDir::RIGHT, EDir::DOWN, EDir::LEFT}) {
                const TPos q = maze.Step(p, d);

                bool qInside = false;
                for (const auto& c : Cells) {
                    if (c == q) {
                        qInside = true;
                        break;
                    }
                }

                if (qInside) {
                    wallsToClear.emplace_back(p, d);
                }
            }
        }
    }

    bool IntersectsReserved(const std::vector<TPos>& reserved) const {
        for (const auto& c : Cells)
            for (const auto& r : reserved)
                if (c == r) return true;
        return false;
    }

   protected:
    std::vector<TPos> Cells;

    static constexpr std::string_view Magic = "[LOCATION]";
};
