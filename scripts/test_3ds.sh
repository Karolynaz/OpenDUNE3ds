#!/usr/bin/env bash
set -eo pipefail
cd "$(dirname "$0")/.."
test_dir=$(mktemp -d /tmp/dune-3ds-tests.XXXXXX)
trap 'rm -rf "$test_dir"' EXIT
common=(-fno-common -std=gnu11 -fsanitize=address,undefined -iquote include -iquote src)
# Compile the actual core routines without pulling the entire world renderer into a
# unit test. This copies complete functions verbatim, never a duplicate implementation.
python3 - "$test_dir/harvester_core.inc" <<'PYCORE'
import re, sys
from pathlib import Path
functions = [('src/tile.c','Tile_Center'), ('src/tile.c','Tile_UnpackTile'), ('src/unit.c','Unit_SetAction'), ('src/unit.c','Unit_SetPosition'),
             ('src/script/structure.c','Script_Structure_RefineSpice'),
             ('src/script/structure.c','Script_Structure_Unknown0C5A')]
with open(sys.argv[1], 'w') as out:
    for filename, name in functions:
        source=Path(filename).read_text()
        start=re.search(r'^(?:bool|uint16|tile32|void) '+name+r'\([^;]*?\)\s*\{',source,re.M).start()
        brace=source.index('{',start); depth=1; end=brace+1
        while depth:
            depth += (source[end]=='{') - (source[end]=='}'); end+=1
        out.write(source[start:end]+'\n')
PYCORE
for test in ui midi speech units paths keyboard harvester; do
    extra=(-I "$test_dir")
    if [[ "$test" == ui || "$test" == midi || "$test" == speech || "$test" == keyboard ]]; then extra=(-I scripts/test_stubs); fi
    if [[ "$test" == units ]]; then
        extra=(-ffunction-sections)
        if [[ $(uname -s) == Darwin ]]; then extra+=(-Wl,-dead_strip); else extra+=(-Wl,--gc-sections); fi
    fi
    clang "${common[@]}" "${extra[@]}" "scripts/test_3ds_${test}.c" -o "$test_dir/$test"
    if [[ "$test" == paths ]]; then (cd "$test_dir" && ./paths); else "$test_dir/$test"; fi
done
