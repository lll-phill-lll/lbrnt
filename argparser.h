#pragma once

#include <charconv>
#include <iostream>

inline void PrintUsage() {
    std::cout << "generate --width W --height H --out state.txt [--openness 0..100] [--seed N]\n"
                 "add-player --state state.txt --name NAME\n"
                 "export-svg --state state.txt --out lbrnt.svg\n";
}

[[noreturn]] inline void ArgError() {
    PrintUsage();
    std::exit(1);
}

template <class T>
struct ArgSpec {
    std::string_view Key;
    T& Value;
    std::optional<T> Default;
};

template <class T>
T ParseValue(std::string_view sv) {
    if constexpr (std::is_same_v<T, int32_t>) {
        int32_t v;
        auto [p, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), v);
        if (ec != std::errc{} || p != sv.data() + sv.size()) ArgError();
        return v;
    } else if constexpr (std::is_same_v<T, std::string>) {
        return std::string{sv};
    } else {
        static_assert(!sizeof(T), "unsupported type");
    }
}

inline std::optional<std::string_view> GetArgValue(int argc, char** argv, std::string_view key) {
    for (int i = 1; i < argc - 1; ++i) {
        if (std::string_view(argv[i]) == key) {
            return std::string_view(argv[i + 1]);
        }
    }
    return std::nullopt;
}

template <class T>
void ParseArgs(int32_t argc, char** argv, std::initializer_list<ArgSpec<T>> specs) {
    for (const auto& s : specs) {
        if (auto raw = GetArgValue(argc, argv, s.Key)) {
            s.Value = ParseValue<T>(*raw);
            continue;
        }
        if (!s.Default) {
            ArgError();
        }
        s.Value = *s.Default;
    }
}
