#include "Location.hpp"
#include "../map.hpp"
#include "Hospital.hpp"
#include "Arsenal.hpp"
#include "Treasure.hpp"
#include "Exit.hpp"

struct NoOpLocation : public Location {
	const char* id() const override { return "none"; }
	void onEnter(Game&, LabyrinthMap&, const std::string&, size_t, size_t, Outcome&) override {}
	void onExit(Game&, LabyrinthMap&, const std::string&, size_t, size_t, Outcome&) override {}
	void onPlaced(Game&, LabyrinthMap&) override {}
};

Location* getLocationFor(CellContent c) {
	static HospitalLocation hospital;
	static ArsenalLocation arsenal;
	static TreasureLocation treasure;
	static ExitLocation exitLoc;
	static NoOpLocation noop;
	switch (c) {
		case CellContent::Hospital: return &hospital;
		case CellContent::Arsenal: return &arsenal;
		case CellContent::Treasure: return &treasure;
		case CellContent::Exit: return &exitLoc;
		default: return &noop;
	}
}


