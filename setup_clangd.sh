#!/bin/sh
set -e
cd "$(dirname "$0")"

# Prefer the py launcher: on Windows, python3/python in PATH are often the
# Microsoft Store aliases, which do nothing but print an install message
if command -v py >/dev/null 2>&1 && py -3 -c "" >/dev/null 2>&1; then
    exec py -3 scripts/setup.py clangd "$@"
fi
if command -v python3 >/dev/null 2>&1 && python3 -c "" >/dev/null 2>&1; then
    exec python3 scripts/setup.py clangd "$@"
fi
if command -v python >/dev/null 2>&1 && python -c "" >/dev/null 2>&1; then
    exec python scripts/setup.py clangd "$@"
fi

echo "setup_clangd.sh: no working Python 3 launcher found (tried py -3, python3, python)" >&2
exit 1
