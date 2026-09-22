#!/bin/bash
# Run a Renode script for the Air60 V2 from this directory.
#   ./run.sh keytest.resc      full boot + key press test (about 10 s)
#   ./run.sh run.resc          boot only
# Needs: renode on PATH (or RENODE=/path/to/renode) and the emulation build
#   qmk compile -kb nuphy/air60v2/ansi -km renode
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
qmk_root=$(cd "$here/../../../../.." && pwd)
renode=${RENODE:-renode}
script=${1:-keytest.resc}

elf="$qmk_root/.build/nuphy_air60v2_ansi_renode.elf"
[ -f "$elf" ] || { echo "missing $elf: run 'qmk compile -kb nuphy/air60v2/ansi -km renode' first" >&2; exit 1; }

# Renode resolves Python peripheral paths relative to its own installation,
# so the platform file is generated with absolute paths.
sed "s|@RENODE_DIR@|$here|g" "$here/air60v2.repl" > "$here/air60v2.gen.repl"
# unprogrammed flash reads 0xFF; the emulated EEPROM relies on it
[ -f "$here/ff.bin" ] || python3 -c "open('$here/ff.bin','wb').write(b'\\xff'*0x20000)"

cd "$here"
exec "$renode" --disable-gui --console --plain "$here/$script"
