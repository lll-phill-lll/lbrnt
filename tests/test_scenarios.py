"""
Проигрывание сценариев из tests/scenarios/**/scenario.json
Запуск: из корня репозитория  pytest tests/
При отсутствии build/labyrinth выполняется cmake -B build и cmake --build build.
Требуется: cmake, компилятор C++ и pip install pytest
"""
from __future__ import annotations

import subprocess
import sys
import warnings
from pathlib import Path

import pytest

TESTS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(TESTS_DIR))

import scenario_lib as scn  # noqa: E402

ROOT = TESTS_DIR.parent

LAB = ROOT / "build" / "labyrinth"


def _ensure_lab_binary() -> None:
    """Если бинарника нет — при необходимости конфигурируем и собираем CMake-проект."""
    if LAB.is_file():
        return
    cache = ROOT / "build" / "CMakeCache.txt"
    if not cache.is_file():
        configure = subprocess.run(
            ["cmake", "-S", ".", "-B", "build"],
            cwd=ROOT,
            capture_output=True,
            text=True,
        )
        if configure.returncode != 0:
            pytest.fail(
                "cmake -S . -B build не удался (нужен cmake и компилятор C++):\n"
                + (configure.stderr or configure.stdout or "")
            )
    build = subprocess.run(
        ["cmake", "--build", "build"],
        cwd=ROOT,
        capture_output=True,
        text=True,
    )
    if build.returncode != 0 or not LAB.is_file():
        pytest.fail(
            f"cmake --build build не собрал {LAB}:\n"
            + (build.stderr or build.stdout or "")
        )


@pytest.fixture(scope="session")
def lab_binary() -> Path:
    _ensure_lab_binary()
    return LAB


def _scenario_dirs():
    return scn.discover_scenario_dirs()


@pytest.mark.parametrize(
    "scenario_dir",
    _scenario_dirs(),
    ids=lambda p: p.relative_to(scn.scenarios_dir()).as_posix(),
)
def test_scenario_replay(scenario_dir: Path, lab_binary: Path, canonize: bool):
    result = scn.run_all_steps(lab_binary, scenario_dir, canonize=canonize)
    if not result.get("ok"):
        msg = result.get("error") or str(result)
        sid = result.get("id", scenario_dir)
        pytest.fail(f"[{sid}]\n{msg}")
    if result.get("canonized"):
        sid = result.get("id", scenario_dir)
        j = result.get("script_index")
        warnings.warn(
            f"Обновлён эталон stdout в {sid} (script[{j}]): перепроверьте diff в scenario.json",
            UserWarning,
            stacklevel=2,
        )


def test_at_least_one_example():
    assert _scenario_dirs(), "Добавьте tests/scenarios/**/scenario.json (например weapons/my_case)"
