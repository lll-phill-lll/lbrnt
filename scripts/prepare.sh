#!/bin/sh

LAB="../build/labyrinth"
STATE="state.txt"
SVG="maze.svg"
SLEEP_DEFAULT="${SLEEP_DEFAULT:-1}"

svg() { "$LAB" export-svg --state "$STATE" --out "${1:-$SVG}"; }
waitf() { sleep "${1:-$SLEEP_DEFAULT}"; }
gen() { w="$1"; h="$2"; o="${3:-0.5}"; s="${4:-42}"; "$LAB" generate --width "$w" --height "$h" --out "$STATE" --openness "$o" --seed "$s"; }
show() { "$LAB" show --state "$STATE" ${1:+"$1"}; }
add_pr() { "$LAB" add-player-random --state "$STATE" --name "$1"; }
add_p() { "$LAB" add-player --state "$STATE" --name "$1" --x "$2" --y "$3"; }
add_item() { "$LAB" add-item --state "$STATE" --item "$1" --x "$2" --y "$3" ${4:+--charges "$4"}; }
add_item_rnd() { "$LAB" add-item-random --state "$STATE" --item "$1" ${2:+--charges "$2"}; }
step() { "$LAB" move --state "$STATE" --name "$1" "$2" && svg && waitf; }
steps() { n="${3:-1}"; i=0; while [ "$i" -lt "$n" ]; do step "$1" "$2"; i=$((i+1)); done; }
hit() { "$LAB" attack --state "$STATE" --name "$1" "$2" && svg && waitf; }
use_item() { "$LAB" use-item --state "$STATE" --name "$1" --item "$2" "$3" && svg && waitf; }
set_cell() { "$LAB" set-cell --state "$STATE" --x "$1" --y "$2" "$3"; }
set_vwall() { "$LAB" set-vwall --state "$STATE" --x "$1" --y "$2" --present "$3"; }
set_hwall() { "$LAB" set-hwall --state "$STATE" --x "$1" --y "$2" --present "$3"; }

gen 7 7 0.5 44
add_pr Rus2m 
add_pr M1sha
add_pr Dasha
add_pr Orino

add_item_rnd shotgun
add_item_rnd rifle
add_item_rnd flashlight
add_item_rnd armor

waitf
svg

steps M1sha up 1
steps M1sha left 2
steps Orino right 1
steps Orino left 5
steps Orino up 1
steps Orino down 1

use_item Orino flashlight up

steps Orino right 5
steps Dasha left 1
steps Dasha down 2

use_item Dasha knife down 

steps Dasha left 1 
steps Dasha right 1 
steps Dasha down 1

steps Orino up 2
use_item Orino flashlight up
use_item Orino shotgun up

