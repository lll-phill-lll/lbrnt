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
                           {"--seed", seed, 0},
                           {"--openness", openness, 50},
                       });

    ParseArgs<std::string>(argc, argv,
                           {
                               {"--out", out, std::nullopt},
                           });
    TGame game(TGameCfg{height, width, openness});
    game.ToFile(out);
    return 0;
}

int32_t HandleAddPlayer() { return 0; }

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
        HandleAddPlayer();
    } else if (cmd == "export-svg") {
        HandleExportSvg(argc, argv);
    } else {
        PrintUsage();
        return 0;
    }

    return 0;
}
