#pragma once

#include <cassert>
#include <iomanip>
#include <iostream>
#include <random>
#include <vector>

#include "common.h"

struct TMazeCfg {
    int32_t W;
    int32_t H;
};

enum class EDir : uint8_t { UP = 0, RIGHT = 1, DOWN = 2, LEFT = 3 };
enum class ECellContent : uint8_t { EMPTY, HOSPITAL };

struct TCell {
    uint8_t Walls = 0b1111;
    ECellContent Content = ECellContent::EMPTY;

    bool HasWall(EDir dir) const { return Walls & (1u << static_cast<uint8_t>(dir)); }
    void RemoveWall(EDir dir) { Walls &= ~(1u << static_cast<uint8_t>(dir)); }
};

class TMaze {
   public:
    TMaze() : W(0), H(0), Cells(0) {}
    TMaze(const TMazeCfg& cfg) : W(cfg.W), H(cfg.H), Cells(W * H) { Generate(); }
    TMaze(std::istream& in) { FromStream(in); }

    int32_t GetH() const { return H; }
    int32_t GetW() const { return W; }

    bool InBounds(TPos pos) const { return 0 <= pos.X && pos.X < W && 0 <= pos.Y && pos.Y < H; }

    TCell& At(TPos p) { return Cells[Idx(p)]; }
    const TCell& At(TPos p) const { return Cells[Idx(p)]; }

    void Generate() {
        assert(W > 0 && H > 0);

        // rebuild just in case
        for (auto& c : Cells) c.Walls = 0b1111;
        std::vector<bool> visited(Cells.size(), false);

        std::random_device rng;
        Dfs({0, 0}, visited, rng);
    }

    const std::string& GetMagic() const { return Magic; }

    void ToStream(std::ostream& out) const {
        out << W << " " << H << std::endl;

        out << std::hex << std::nouppercase << std::setfill('0');
        for (int y = 0; y < H; ++y) {
            for (int x = 0; x < W; ++x) {
                uint8_t w = At({x, y}).Walls;
                out << std::setw(2) << static_cast<unsigned>(w);
            }
            out << std::endl;
        }
    }

    void FromStream(std::istream& in) {
        if (!(in >> W >> H)) {
            std::cerr << "Failed to read header" << std::endl;
            std::exit(1);
        }
        if (W <= 0 || H <= 0) {
            std::cerr << "Invalid maze size in header" << std::endl;
            std::exit(1);
        }

        Cells.resize(H * W);

        std::string line;
        std::getline(in, line);

        for (int y = 0; y < H; ++y) {
            if (!std::getline(in, line)) {
                std::cerr << "Unexpected end of input while reading rows" << std::endl;
                std::exit(1);
            }

            std::string hex;
            hex.reserve(line.size());
            for (char c : line)
                if (!std::isspace(static_cast<unsigned char>(c))) hex.push_back(c);

            if ((int)hex.size() < 2 * W) {
                std::cerr << "Row " + std::to_string(y) + " is too short" << std::endl;
                std::exit(1);
            }

            for (int x = 0; x < W; ++x) {
                uint8_t walls = ParseHexByte(hex[2 * x], hex[2 * x + 1]);
                walls &= 0b1111;
                Cells[y * W + x].Walls = walls;
            }
        }
    }

   private:
    TPos step(TPos pos, EDir dir) {
        switch (dir) {
            case EDir::UP:
                return {pos.X, pos.Y - 1};
            case EDir::RIGHT:
                return {pos.X + 1, pos.Y};
            case EDir::DOWN:
                return {pos.X, pos.Y + 1};
            case EDir::LEFT:
                return {pos.X - 1, pos.Y};
        }
        assert(false);
        return {};
    }

    EDir OppositeDir(EDir dir) {
        switch (dir) {
            case EDir::UP:
                return EDir::DOWN;
            case EDir::RIGHT:
                return EDir::LEFT;
            case EDir::DOWN:
                return EDir::UP;
            case EDir::LEFT:
                return EDir::RIGHT;
        }
        assert(false);
        return {};
    }

    int Idx(TPos pos) const { return pos.Y * W + pos.X; }

    void RemoveWall(TPos from_pos, EDir dir) {
        assert(InBounds(from_pos));
        TPos to_pos = step(from_pos, dir);
        assert(InBounds(to_pos));

        At(from_pos).RemoveWall(dir);
        At(to_pos).RemoveWall(OppositeDir(dir));
    }

    void Dfs(TPos pos, std::vector<bool>& visited, std::random_device& rng) {
        visited[Idx(pos)] = true;

        std::array<EDir, 4> dirs{EDir::UP, EDir::RIGHT, EDir::DOWN, EDir::LEFT};
        std::shuffle(dirs.begin(), dirs.end(), rng);

        for (EDir dir : dirs) {
            TPos next_pos = step(pos, dir);
            if (!InBounds(next_pos)) continue;
            if (visited[Idx(next_pos)]) continue;

            RemoveWall(pos, dir);
            // TODO: use stack
            Dfs(next_pos, visited, rng);
        }
    }

    uint8_t HexVal(char c) {
        if ('0' <= c && c <= '9') return uint8_t(c - '0');
        if ('a' <= c && c <= 'f') return uint8_t(10 + (c - 'a'));
        if ('A' <= c && c <= 'F') return uint8_t(10 + (c - 'A'));
        assert(false);
        return {};
    }

    uint8_t ParseHexByte(char hi, char lo) { return uint8_t((HexVal(hi) << 4) | HexVal(lo)); }

   private:
    int32_t W = 0;
    int32_t H = 0;
    std::vector<TCell> Cells;

    const std::string Magic = "[MAZE]";
};
