#!/bin/sh
DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
VENV_DIR="$DIR/.venv"

if [ ! -d "$VENV_DIR" ]; then
	python3 -m venv "$VENV_DIR" || exit 1
fi

"$VENV_DIR/bin/python" -m pip install --upgrade pip setuptools wheel || exit 1
if [ -f "$DIR/requirements.txt" ]; then
	"$VENV_DIR/bin/python" -m pip install -r "$DIR/requirements.txt" || exit 1
fi

exec "$VENV_DIR/bin/python" "$DIR/webui.py"


