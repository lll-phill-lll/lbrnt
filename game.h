#pragma once

#include <fstream>

#include "maze.h"

struct TGameCfg {
    int32_t W;
    int32_t H;
};

class TGame {
   public:
    TGame(const std::string& path) { FromFile(path); }
    TGame(const TGameCfg& cfg) : Maze({cfg.W, cfg.H}) {}

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

   private:
    bool IsValidMagic(const std::string& magic) const { return magic.front() == '[' && magic.back() == ']'; }

   private:
    TMaze Maze;
};
