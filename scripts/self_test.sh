#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
echo '== Python syntax =='
python3 -m compileall -q raspberry_pi tests
echo 'PASS'
echo '== Unit tests =='
python3 -m pytest -q tests
echo '== JSON config =='
python3 -m json.tool config/settings.example.json >/dev/null
echo 'PASS'
echo '== Shell syntax =='
for f in install.sh scripts/*.sh; do bash -n "$f"; done
echo 'PASS'
echo 'SELF TEST PASS'
