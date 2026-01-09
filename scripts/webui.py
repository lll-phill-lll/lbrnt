#!/usr/bin/env python3
import os
import subprocess
from flask import Flask, request, jsonify, send_from_directory, Response

# Paths
THIS_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(THIS_DIR)
LAB = os.path.join(ROOT, "build", "labyrinth")
STATE = os.path.join(ROOT, "state.txt")
SVG = os.path.join(ROOT, "maze.svg")

app = Flask(
    __name__,
    static_folder=os.path.join(THIS_DIR, "static"),
    static_url_path="/static",
)

@app.after_request
def add_no_store(resp):
    resp.headers["Cache-Control"] = "no-store"
    return resp

def run_lab(args):
    """Run labyrinth CLI with given args."""
    res = subprocess.run([LAB] + args, capture_output=True, text=True)
    return res.returncode, res.stdout.strip(), res.stderr.strip()

def split_lines(text: str):
    """Normalize and split text into lines, converting literal '\\n' to newlines."""
    if text is None:
        return []
    t = str(text)
    t = t.replace("\r\n", "\n").replace("\r", "\n")
    # repeatedly unescape literal \n
    while "\\n" in t:
        t = t.replace("\\n", "\n")
    return t.split("\n")

def ensure_svg():
    """Ensure SVG exists by exporting current state."""
    if not os.path.exists(STATE):
        return False, "state.txt not found"
    code, out, err = run_lab(["export-svg", "--state", STATE, "--out", SVG])
    if code != 0:
        return False, err or out or "export-svg failed"
    return True, out


def read_svg_text():
    if not os.path.exists(SVG):
        ok, _ = ensure_svg()
        if not ok:
            return None
    with open(SVG, "r", encoding="utf-8") as f:
        return f.read()

def parse_turn_info():
    """Parse turn info from state.txt (TURNS block)."""
    info = {"enforce": False, "index": 0, "order": [], "current": None, "next": None}
    if not os.path.exists(STATE):
        return info
    try:
        with open(STATE, "r", encoding="utf-8") as f:
            lines = [l.rstrip("\n") for l in f]
        # find TURNS line
        pos = None
        for i, line in enumerate(lines):
            if line.startswith("TURNS "):
                pos = i
                break
        if pos is None:
            return info
        parts = lines[pos].split()
        # TURNS enf idx cnt
        if len(parts) < 4:
            return info
        enf = int(parts[1])
        idx = int(parts[2])
        cnt = int(parts[3])
        order = []
        for j in range(cnt):
            if pos + 1 + j < len(lines):
                order.append(lines[pos + 1 + j].strip())
        info["enforce"] = (enf != 0)
        info["index"] = idx if 0 <= idx < len(order) else 0
        info["order"] = order
        info["current"] = order[info["index"]] if order else None
        if order:
            info["next"] = order[(info["index"] + 1) % len(order)]
        else:
            info["next"] = None
    except Exception:
        pass
    return info


def parse_players():
    """Parse player names from state.txt."""
    names = []
    if not os.path.exists(STATE):
        return names
    try:
        with open(STATE, "r", encoding="utf-8") as f:
            lines = [l.rstrip("\n") for l in f]
        # Find 'PLAYERS N'
        idx = next((i for i, l in enumerate(lines) if l.startswith("PLAYERS ")), None)
        if idx is None:
            return names
        try:
            n = int(lines[idx].split()[1])
        except Exception:
            return names
        for j in range(n):
            if idx + 1 + j >= len(lines):
                break
            parts = lines[idx + 1 + j].split()
            if parts:
                names.append(parts[0])
    except Exception:
        pass
    return names


@app.get("/")
def root_index():
    return send_from_directory(app.static_folder, "index.html")


@app.get("/api/svg")
def api_svg():
    svg = read_svg_text()
    if svg is None:
        return jsonify({"ok": False, "error": "SVG not available (no state)"}), 400
    return Response(svg, mimetype="image/svg+xml", headers={"Cache-Control": "no-store"})

@app.get("/api/turn")
def api_turn():
    return jsonify({"ok": True, "turn": parse_turn_info()})


@app.get("/api/players")
def api_players():
    return jsonify({"ok": True, "players": parse_players()})


@app.post("/api/generate")
def api_generate():
    width = request.values.get("width", type=int, default=12)
    height = request.values.get("height", type=int, default=12)
    openness = request.values.get("openness", type=float, default=0.5)
    seed = request.values.get("seed", type=int, default=42)
    code, out, err = run_lab([
        "generate",
        "--width", str(width),
        "--height", str(height),
        "--out", STATE,
        "--openness", str(openness),
        "--seed", str(seed),
    ])
    ok, msg = ensure_svg()
    return jsonify({
        "ok": code == 0 and ok,
        "stdout": out,
        "stderr": err,
        "stdout_lines": split_lines(out),
        "stderr_lines": split_lines(err),
        "players": parse_players(),
        "turn": parse_turn_info(),
    }), (200 if code == 0 and ok else 400)


@app.post("/api/add-player-random")
def api_add_player_random():
    name = request.values.get("name", "").strip()
    if not name:
        return jsonify({"ok": False, "error": "name required"}), 400
    code, out, err = run_lab(["add-player-random", "--state", STATE, "--name", name])
    ok, msg = ensure_svg()
    return jsonify({
        "ok": code == 0 and ok,
        "stdout": out,
        "stderr": err,
        "stdout_lines": split_lines(out),
        "stderr_lines": split_lines(err),
        "players": parse_players(),
        "turn": parse_turn_info(),
    }), (200 if code == 0 and ok else 400)


@app.post("/api/move")
def api_move():
    name = request.values.get("name", "").strip()
    direction = request.values.get("dir", "").strip()
    if not name or direction not in ("up", "down", "left", "right"):
        return jsonify({"ok": False, "error": "name and dir required"}), 400
    code, out, err = run_lab(["move", "--state", STATE, "--name", name, direction])
    ok, msg = ensure_svg()
    return jsonify({
        "ok": code == 0 and ok,
        "stdout": out,
        "stderr": err,
        "stdout_lines": split_lines(out),
        "stderr_lines": split_lines(err),
        "players": parse_players(),
        "turn": parse_turn_info(),
    }), (200 if code == 0 and ok else 400)


@app.post("/api/attack")
def api_attack():
    name = request.values.get("name", "").strip()
    direction = request.values.get("dir", "").strip()
    if not name or direction not in ("up", "down", "left", "right"):
        return jsonify({"ok": False, "error": "name and dir required"}), 400
    code, out, err = run_lab(["attack", "--state", STATE, "--name", name, direction])
    ok, msg = ensure_svg()
    return jsonify({
        "ok": code == 0 and ok,
        "stdout": out,
        "stderr": err,
        "stdout_lines": split_lines(out),
        "stderr_lines": split_lines(err),
        "players": parse_players(),
        "turn": parse_turn_info(),
    }), (200 if code == 0 and ok else 400)


@app.post("/api/use-item")
def api_use_item():
    name = request.values.get("name", "").strip()
    item = request.values.get("item", "").strip()
    direction = request.values.get("dir", "").strip()
    if not name or item not in ("knife", "shotgun", "rifle", "flashlight") or direction not in ("up", "down", "left", "right"):
        return jsonify({"ok": False, "error": "name, item and dir required"}), 400
    code, out, err = run_lab(["use-item", "--state", STATE, "--name", name, "--item", item, direction])
    ok, msg = ensure_svg()
    return jsonify({
        "ok": code == 0 and ok,
        "stdout": out,
        "stderr": err,
        "stdout_lines": split_lines(out),
        "stderr_lines": split_lines(err),
        "players": parse_players(),
        "turn": parse_turn_info(),
    }), (200 if code == 0 and ok else 400)


@app.post("/api/preset")
def api_preset():
    """Generate map, add 4 players, place one of each item, export SVG."""
    width = request.values.get("width", type=int, default=12)
    height = request.values.get("height", type=int, default=12)
    openness = request.values.get("openness", type=float, default=0.5)
    seed = request.values.get("seed", type=int, default=42)
    names_raw = request.values.get("names", default="Rus2m,M1sha,Dasha,Orino")
    names = [n.strip() for n in names_raw.split(",") if n.strip()]
    if len(names) < 4:
        # Pad unique defaults if fewer than 4 provided
        defaults = ["Rus2m", "M1sha", "Dasha", "Orino"]
        for d in defaults:
            if len(names) >= 4:
                break
            if d not in names:
                names.append(d)
    # Generate
    code, out_g, err_g = run_lab([
        "generate",
        "--width", str(width),
        "--height", str(height),
        "--out", STATE,
        "--openness", str(openness),
        "--seed", str(seed),
    ])
    if code != 0:
        return jsonify({"ok": False, "stderr": err_g, "stdout": out_g}), 400
    # Add players
    outs = [out_g]; errs = [err_g]
    for nm in names[:4]:
        c, o, e = run_lab(["add-player-random", "--state", STATE, "--name", nm])
        outs.append(o); errs.append(e)
        if c != 0:
            ok, _ = ensure_svg()
            return jsonify({"ok": False, "stderr": "\n".join(errs), "stdout": "\n".join(outs)}), 400
    # Place one of each item randomly
    for item in ("shotgun", "rifle", "flashlight", "knife"):
        c, o, e = run_lab(["add-item-random", "--state", STATE, "--item", item])
        outs.append(o); errs.append(e)
        if c != 0:
            ok, _ = ensure_svg()
            return jsonify({"ok": False, "stderr": "\n".join(errs), "stdout": "\n".join(outs)}), 400
    ok, msg = ensure_svg()
    return jsonify({
        "ok": ok,
        "stdout": "\n".join(outs),
        "stderr": "\n".join(errs),
        "stdout_lines": split_lines("\n".join(outs)),
        "stderr_lines": split_lines("\n".join(errs)),
        "players": parse_players(),
        "turn": parse_turn_info(),
    }), (200 if ok else 400)


@app.get("/healthz")
def healthz():
    return jsonify({"ok": True})


if __name__ == "__main__":
    port = int(os.environ.get("PORT", "8000"))
    app.run(host="127.0.0.1", port=port, debug=False)


