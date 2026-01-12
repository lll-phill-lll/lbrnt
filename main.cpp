#include "argparser.h"
#include "game.h"

int32_t HandleGenerate(int32_t argc, char** argv) {
    int32_t width;
    int32_t height;
    std::string out;
    int32_t seed;
    int32_t openness;

    ParseArgs<int32_t>(argc, argv,
                       {
                           {"--width", width, std::nullopt},
                           {"--height", height, std::nullopt},
                           {"--seed", seed, 42},
                           {"--openness", openness, 50},
                       });

    ParseArgs<std::string>(argc, argv,
                           {
                               {"--out", out, std::nullopt},
                           });
    TGame game(TGameCfg{width, height, openness, static_cast<uint64_t>(seed)});
    game.ToFile(out);
    return 0;
}

int32_t HandleAddPlayer(int32_t argc, char** argv) {
    std::string state;
    std::string name;
    ParseArgs<std::string>(argc, argv, {{"--state", state, std::nullopt}, {"--name", name, std::nullopt}});
    TGame game(state);
    auto msgs = game.AddPlayerRandom(name);
    for (const auto& m : msgs) std::cout << m << "\n";
    game.ToFile(state);
    return 0;
}

int32_t HandleMovePlayer(int32_t argc, char** argv) {
    std::string state;
    std::string name;
    std::string dir;
    ParseArgs<std::string>(argc, argv, {{"--state", state, std::nullopt}, {"--name", name, std::nullopt}, {"--dir", dir, std::nullopt}});
    TGame game(state);
    auto msgs = game.MovePlayer(name, dir);
    for (const auto& m : msgs) std::cout << m << "\n";
    game.ToFile(state);
    return 0;
}

int32_t HandleExportSvg(int32_t argc, char** argv) {
    std::string state;
    std::string out;

    ParseArgs<std::string>(argc, argv,
                           {
                               {"--state", state, std::nullopt},
                               {"--out", out, std::nullopt},
                           });

    TGame game(state);
    game.ToSvg(out);

    return 0;
}

int32_t main(int32_t argc, char** argv) {
    if (argc < 2) {
        PrintUsage();
        return 0;
    }

    std::string cmd = argv[1];
    if (cmd == "generate") {
        return HandleGenerate(argc, argv);
    } else if (cmd == "add-player") {
        return HandleAddPlayer(argc, argv);
    } else if (cmd == "move-player") {
        return HandleMovePlayer(argc, argv);
    } else if (cmd == "export-svg") {
        HandleExportSvg(argc, argv);
    } else {
        PrintUsage();
        return 0;
    }

    return 0;
}
