cmake_minimum_required(VERSION 3.16)
project(labyrinth LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(labyrinth
	main.cpp
	map.hpp
	map.cpp
	generator.hpp
	generator.cpp
	game.hpp
	game.cpp
	state.hpp
	state.cpp
	viz.hpp
	viz.cpp
	items/Item.hpp
	items/Knife.hpp
	items/Knife.cpp
	items/Shotgun.hpp
	items/Shotgun.cpp
	items/Rifle.hpp
	items/Rifle.cpp
	items/Flashlight.hpp
	items/Flashlight.cpp
	locations/LocationUtils.hpp
	locations/LocationUtils.cpp
	locations/Location.hpp
	locations/Location.cpp
	locations/Treasure.hpp
	locations/Treasure.cpp
	locations/Exit.hpp
	locations/Exit.cpp
	locations/Hospital.hpp
	locations/Hospital.cpp
	locations/Arsenal.hpp
	locations/Arsenal.cpp
)

if (APPLE)
	target_compile_options(labyrinth PRIVATE -Wall -Wextra -Wpedantic)
else()
	target_compile_options(labyrinth PRIVATE -Wall -Wextra -Wpedantic)
endif()


