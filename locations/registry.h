#pragma once

#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "arsenal.h"
#include "base.h"
#include "hospital.h"

class TLocationRegistry {
   public:
    using TFactory = std::function<std::unique_ptr<ILocation>()>;

    void Register(std::string_view type, TFactory factory) {
        Factories.emplace_back(std::string(type), std::move(factory));
    }

    std::unique_ptr<ILocation> Create(std::string_view type) const {
        for (const auto& [t, f] : Factories) {
            if (t == type) return f();
        }
        return nullptr;
    }

    std::unique_ptr<ILocation> ReadOne(std::istream& in) const {
        std::string type;
        if (!(in >> type)) {
            std::cerr << "Failed to read location type" << std::endl;
            std::exit(1);
        }
        auto loc = Create(type);
        if (!loc) {
            std::cerr << "Failed to create location " << type << std::endl;
            std::exit(1);
        }
        loc->FromStream(in);
        return loc;
    }

    const std::vector<std::pair<std::string, TFactory>>& All() const { return Factories; }

   private:
    std::vector<std::pair<std::string, TFactory>> Factories;
};

// TODO: do more like a singleton
inline TLocationRegistry& GetLocationRegistry() {
    static TLocationRegistry instance;
    return instance;
}

struct TLocationAutoRegister {
    TLocationAutoRegister(std::string_view type, TLocationRegistry::TFactory factory) {
        GetLocationRegistry().Register(type, std::move(factory));
    }
};

inline TLocationAutoRegister _AutoRegHospital{"HOSPITAL", [] { return std::make_unique<THospital>(); }};
inline TLocationAutoRegister _AutoRegArsenal{"ARSENAL", [] { return std::make_unique<TArsenal>(); }};
