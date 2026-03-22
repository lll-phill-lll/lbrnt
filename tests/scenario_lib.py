"""
Сценарии из tests/scenarios/<категория>/<id>/scenario.json (или tests/scenarios/<id>/scenario.json).
setup-команды + script (игровые действия). Проверка только вывода движка (stdout/stderr).

Канонизация эталона: pytest ``--canonize`` — при расхождении только stdout обновляет
``expect_stdout`` в файле (см. save_scenario, run_scenario(..., canonize=True)).
"""
from __future__ import annotations

import json
import os
import subprocess
import tempfile
from pathlib import Path
from typing import Any

SCENARIO_FILE = "scenario.json"


def repo_root() -> Path:
    """Корень репозитория (родитель каталога tests/)."""
    return Path(__file__).resolve().parent.parent


def scenarios_dir() -> Path:
    return repo_root() / "tests" / "scenarios"


def scenario_rel_id(scenario_dir: Path) -> str:
    """Идентификатор сценария для отчётов: `weapons/knife_to_hospital`."""
    return scenario_dir.resolve().relative_to(scenarios_dir().resolve()).as_posix()


def safe_join(scenario_dir: Path, rel: str) -> Path:
    base = scenario_dir.resolve()
    p = (base / rel).resolve()
    if not str(p).startswith(str(base) + os.sep) and p != base:
        raise ValueError("path escapes scenario dir")
    return p


def load_scenario(scenario_dir: Path) -> dict[str, Any]:
    p = scenario_dir / SCENARIO_FILE
    if not p.is_file():
        raise FileNotFoundError(p)
    with open(p, encoding="utf-8") as f:
        return json.load(f)


def save_scenario(scenario_dir: Path, data: dict[str, Any]) -> None:
    """Записать scenario.json с отступами (как при ручном редактировании)."""
    p = scenario_dir / SCENARIO_FILE
    with open(p, "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, indent=2)
        f.write("\n")


def norm_text(s: str) -> str:
    t = (s or "").replace("\r\n", "\n").replace("\r", "\n")
    return "\n".join(line.rstrip() for line in t.split("\n")).strip()


def _short_text(s: str, limit: int = 2000) -> str:
    t = s or ""
    if len(t) <= limit:
        return t
    return t[:limit] + f"\n… (обрезано, всего {len(t)} символов)"


def _append_step_output(accum: str, piece: str) -> str:
    """Склеить вывод шагов сценария (как при записи expect_stdout в capture-final)."""
    p = (piece or "").strip()
    if not p:
        return accum
    return accum + ("\n" if accum else "") + p


def _step_has_stdout_expect(step: dict[str, Any]) -> bool:
    if step.get("expect_stdout") is not None:
        return True
    subs = step.get("expect_stdout_contains")
    return isinstance(subs, list) and len(subs) > 0


def _format_cli_failure(title: str, code: int, stdout: str, stderr: str) -> str:
    """Человекочитаемое сообщение при ненулевом коде выхода CLI."""
    parts = [f"{title}: процесс завершился с кодом {code}."]
    err = (stderr or "").strip()
    out = (stdout or "").strip()
    if err:
        parts.append("stderr:\n" + _short_text(err, 1500))
    if out and out != err:
        parts.append("stdout:\n" + _short_text(out, 1500))
    if not err and not out:
        parts.append("(нет вывода в stdout/stderr)")
    return "\n\n".join(parts)


def run_lab(lab: Path, args: list[str]) -> tuple[int, str, str]:
    res = subprocess.run([str(lab)] + args, capture_output=True, text=True)
    return res.returncode, (res.stdout or "").strip(), (res.stderr or "").strip()


def build_setup_argv(state_path: str, action: dict[str, Any]) -> list[str]:
    t = action.get("type")
    if t == "generate":
        args = [
            "generate",
            "--width",
            str(int(action["width"])),
            "--height",
            str(int(action["height"])),
            "--out",
            state_path,
        ]
        if "openness" in action:
            args += ["--openness", str(float(action["openness"]))]
        if "seed" in action:
            args += ["--seed", str(int(action["seed"]))]
        if "turns" in action:
            args += ["--turns", "1" if action["turns"] else "0"]
        if "turn_actions" in action:
            args += ["--turn-actions", str(int(action["turn_actions"]))]
        if action.get("bot_steps"):
            args += ["--bot-steps", str(int(action["bot_steps"]))]
        return args
    if t == "add-player":
        return [
            "add-player",
            "--state",
            state_path,
            "--name",
            str(action["name"]),
            "--x",
            str(int(action["x"])),
            "--y",
            str(int(action["y"])),
        ]
    if t == "add-player-random":
        return ["add-player-random", "--state", state_path, "--name", str(action["name"])]
    if t == "set-turns":
        return ["set-turns", "--state", state_path, "1" if action.get("on", True) else "0"]
    if t == "init-turns":
        return ["init-turns", "--state", state_path]
    if t == "init-base":
        return ["init-base", "--state", state_path]
    if t == "give-item":
        args = ["give-item", "--state", state_path, "--name", str(action["name"]), "--item", str(action["item"])]
        if action.get("charges") is not None:
            args += ["--charges", str(int(action["charges"]))]
        return args
    if t == "give-treasure":
        # то же, что give-item с treasure (совместимость со старыми scenario.json)
        return [
            "give-item",
            "--state",
            state_path,
            "--name",
            str(action["name"]),
            "--item",
            "treasure",
            "--charges",
            "1",
        ]
    if t == "add-item":
        args = [
            "add-item",
            "--state",
            state_path,
            "--item",
            str(action["item"]),
            "--x",
            str(int(action["x"])),
            "--y",
            str(int(action["y"])),
        ]
        if action.get("charges") is not None:
            args += ["--charges", str(int(action["charges"]))]
        return args
    if t == "add-item-random":
        args = ["add-item-random", "--state", state_path, "--item", str(action["item"])]
        if action.get("charges") is not None:
            args += ["--charges", str(int(action["charges"]))]
        return args
    if t == "set-cell":
        return [
            "set-cell",
            "--state",
            state_path,
            "--x",
            str(int(action["x"])),
            "--y",
            str(int(action["y"])),
            str(action["cell"]),
        ]
    raise ValueError(f"unknown setup type: {t}")


def build_game_argv(state_path: str, action: dict[str, Any]) -> list[str]:
    t = action.get("type")
    if t == "move":
        return ["move", "--state", state_path, "--name", action["name"], action["dir"]]
    if t == "attack":
        return ["attack", "--state", state_path, "--name", action["name"], action["dir"]]
    if t == "use-item":
        return [
            "use-item",
            "--state",
            state_path,
            "--name",
            action["name"],
            "--item",
            action["item"],
            action["dir"],
        ]
    if t == "player-status":
        return ["player-status", "--state", state_path, "--name", str(action["name"])]
    raise ValueError(f"unknown game action type: {t}")


def _check_expect(
    stdout_acc: str,
    stderr_acc: str,
    step: dict[str, Any],
) -> tuple[bool, list[str]]:
    checks: list[str] = []
    ok = True
    if step.get("expect_stdout") is not None:
        exp = norm_text(str(step["expect_stdout"]))
        act = norm_text(stdout_acc)
        if exp != act:
            checks.append(
                "Несовпадение expect_stdout.\n"
                "Ожидалось:\n"
                f"{_short_text(exp)}\n\n"
                "Фактически (stdout за шаг, после resolve-bots):\n"
                f"{_short_text(act)}"
            )
            ok = False
    else:
        subs = step.get("expect_stdout_contains") or []
        if not isinstance(subs, list):
            subs = []
        for sub in subs:
            if sub and sub not in stdout_acc:
                checks.append(
                    f"В stdout нет ожидаемой подстроки {sub!r}.\n"
                    f"Фактический stdout:\n{_short_text(stdout_acc)}"
                )
                ok = False
    esubs = step.get("expect_stderr_contains") or []
    if not isinstance(esubs, list):
        esubs = []
    for sub in esubs:
        if sub and sub not in stderr_acc:
            checks.append(
                f"В stderr нет ожидаемой подстроки {sub!r}.\n"
                f"Фактический stderr:\n{_short_text(stderr_acc)}"
            )
            ok = False
    return ok, checks


def _checks_are_only_stdout_expect_failures(checks: list[str]) -> bool:
    """Можно канонизировать: только расхождение expect_stdout / expect_stdout_contains (не stderr, не CLI)."""
    if not checks:
        return False
    for c in checks:
        if "Несовпадение expect_stdout" in c:
            continue
        if "В stdout нет ожидаемой подстроки" in c:
            continue
        return False
    return True


def _apply_canonical_stdout_to_step(step: dict[str, Any], acc_out: str) -> None:
    """Записать фактический stdout как эталон (после norm_text). Убирает expect_stdout_contains — переход на точное совпадение."""
    step["expect_stdout"] = norm_text(acc_out)
    step.pop("expect_stdout_contains", None)


def run_scenario(lab: Path, scenario_dir: Path, *, canonize: bool = False) -> dict[str, Any]:
    data = load_scenario(scenario_dir)
    sid = scenario_rel_id(scenario_dir)
    desc = data.get("description") or data.get("title") or sid
    setup = data.get("setup") or []
    script = data.get("script") or []
    if not isinstance(setup, list):
        return {"ok": False, "error": "setup must be a list", "id": sid, "description": desc}
    if not isinstance(script, list):
        return {"ok": False, "error": "script must be a list", "id": sid, "description": desc}

    fd, tmp = tempfile.mkstemp(suffix=".txt", prefix="lab_scn_")
    os.close(fd)
    try:
        for i, cmd in enumerate(setup):
            if not isinstance(cmd, dict):
                return {"ok": False, "error": f"setup[{i}] is not an object", "id": sid, "description": desc}
            try:
                argv = build_setup_argv(tmp, cmd)
            except ValueError as e:
                return {"ok": False, "error": str(e), "id": sid, "description": desc}
            code, out, err = run_lab(lab, argv)
            if code != 0:
                return {
                    "ok": False,
                    "error": _format_cli_failure(f"setup[{i}]", code, out, err),
                    "exit_code": code,
                    "stdout": out,
                    "stderr": err,
                    "id": sid,
                    "description": desc,
                }

        script_results: list[dict[str, Any]] = []
        stdout_script_acc = ""
        stderr_script_acc = ""
        for j, step in enumerate(script):
            if not isinstance(step, dict):
                return {"ok": False, "error": f"script[{j}] is not an object", "id": sid, "description": desc}
            try:
                argv = build_game_argv(tmp, step)
            except ValueError as e:
                return {"ok": False, "error": str(e), "id": sid, "description": desc}
            code, out, err = run_lab(lab, argv)
            seg_out = out
            seg_err = err
            if code != 0:
                return {
                    "ok": False,
                    "error": _format_cli_failure(f"script[{j}]", code, out, err),
                    "exit_code": code,
                    "stdout": out,
                    "stderr": err,
                    "script_index": j,
                    "id": sid,
                    "description": desc,
                }
            if step.get("type") == "player-status":
                acc_out = seg_out
                acc_err = seg_err
            else:
                c2, o2, e2 = run_lab(lab, ["resolve-bots", "--state", tmp])
                if c2 != 0:
                    return {
                        "ok": False,
                        "error": _format_cli_failure(f"resolve-bots после script[{j}]", c2, o2, e2),
                        "exit_code": c2,
                        "stdout": o2,
                        "stderr": e2,
                        "script_index": j,
                        "id": sid,
                        "description": desc,
                    }
                acc_out = seg_out
                if o2:
                    acc_out = acc_out + ("\n" if acc_out else "") + o2
                acc_err = seg_err
                if e2:
                    acc_err = acc_err + ("\n" if acc_err else "") + e2

            stdout_script_acc = _append_step_output(stdout_script_acc, acc_out)
            stderr_script_acc = _append_step_output(stderr_script_acc, acc_err)

            check_out = stdout_script_acc if _step_has_stdout_expect(step) else acc_out
            check_err = stderr_script_acc if _step_has_stdout_expect(step) else acc_err
            ok, checks = _check_expect(check_out, check_err, step)
            script_results.append(
                {
                    "script_index": j,
                    "ok": ok,
                    "stdout": acc_out,
                    "stderr": acc_err,
                    "checks": checks,
                }
            )
            if not ok:
                if (
                    canonize
                    and _checks_are_only_stdout_expect_failures(checks)
                    and isinstance(data.get("script"), list)
                    and j < len(data["script"])
                ):
                    _apply_canonical_stdout_to_step(data["script"][j], check_out)
                    save_scenario(scenario_dir, data)
                    return {
                        "ok": True,
                        "canonized": True,
                        "script_index": j,
                        "stdout": acc_out,
                        "stderr": acc_err,
                        "results": script_results,
                        "id": sid,
                        "description": desc,
                    }
                return {
                    "ok": False,
                    "error": "\n\n".join(checks) if checks else "проверка вывода не прошла",
                    "script_index": j,
                    "stdout": acc_out,
                    "stderr": acc_err,
                    "results": script_results,
                    "id": sid,
                    "description": desc,
                }

        return {
            "ok": True,
            "id": sid,
            "description": desc,
            "results": script_results,
        }
    finally:
        try:
            os.unlink(tmp)
        except OSError:
            pass


def discover_scenario_dirs() -> list[Path]:
    root = scenarios_dir()
    if not root.is_dir():
        return []
    out: list[Path] = []
    for sf in sorted(root.rglob(SCENARIO_FILE)):
        parent = sf.parent
        try:
            rel = parent.relative_to(root)
        except ValueError:
            continue
        if rel == Path("."):
            continue
        out.append(parent)
    return out


def run_all_steps(lab: Path, scenario_dir: Path, *, canonize: bool = False) -> dict[str, Any]:
    """Имя совместимо со старым pytest; теперь это один прогон scenario.json.

    canonize: при несовпадении только stdout-ожиданий записать фактический вывод в scenario.json
    и считать шаг успешным (для обновления эталонов).
    """
    return run_scenario(lab, scenario_dir, canonize=canonize)
