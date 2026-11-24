#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp; from cps2recomp: https://github.com/tmcleroy/cps2recomp
# Checks the repository's files for things that must never be committed (CONTRIBUTING.md): game files, keys,
# anything made from a game, an emulator's sources, personal paths and addresses, large files, the names of ports
# that are not public; that every file marked as another project's is listed in the notices; and that every source
# file has its license and copyright lines. Run it before pushing (make audit); CI runs it on every push and pull
# request. It looks at the files git tracks and the new ones it would add (not those .gitignore leaves out).
# Exits non-zero on a finding.
set -eu
cd "$(git rev-parse --show-toplevel)"
LIST="$(mktemp)"
trap 'rm -f "$LIST"' EXIT
git ls-files -co --exclude-standard | while read -r f; do [ -e "$f" ] && echo "$f"; done > "$LIST"

fail=0
check() { if [ -n "$2" ]; then echo "AUDIT FAIL: $1"; echo "$2" | head -10; fail=1; else echo "ok: $1"; fi; }
# (this script names the things it looks for, so it is not searched itself; nor are third_party/'s files, other
# projects' as they publish them, which only the ones the notices list may be: checked below; nor recomp-common/,
# another repository's copy, which tools/sync-common.sh --check keeps unchanged and which is audited there)
others='^(tools/audit\.sh|third_party/.*|recomp-common/.*)$'
grep_files() { grep -vE "$others" "$LIST" | tr '\n' '\0' | xargs -0 grep -nIE "$@" 2>/dev/null || true; }
grep_files_i() { grep -vE "$others" "$LIST" | tr '\n' '\0' | xargs -0 grep -nIiE "$@" 2>/dev/null || true; }

# (ROM sets are zips; the chips' dumps are named for the chip or its place on the board: u4, u24, ic10, .bin, .rom)
check "no game, key, save or archive file types" "$(grep -iE '\.(zip|7z|rar|gz|tgz|key|bin|rom|chd|state|sav|eep|nv|nand|lst|u[0-9]+|ic[0-9]+|[0-9]{2}[a-z]?)$' "$LIST" || true)"
# (a game's folder holds its own sources, its game.cfg, notes and recipe.txt: the options the checks play it with)
check "nothing made from a game (translated code, recordings, listings, memory dumps)" "$(grep -E '(^|/)(work|build|build-[^/]*)/|(^|/)gen/|(^|/)blocks_[0-9]+\.cpp$|(^|/)registry\.cpp$|\.(dump|ram|sprites|frames|trace|wav)$|(^|/)RECORDING' "$LIST" || true)"
# (a file a tool makes from recordings of the game's play holds the game's own data: its patterns, its palettes)
check "no data recorded from a game's play" "$(grep_files -l -e '^// Made by .* from recordings of play' || true)"
check "every game has its game.cfg" "$(grep -E '^games/[^/]+/' "$LIST" | cut -d/ -f2 | sort -u | while read -r g; do [ -f "games/$g/game.cfg" ] || echo "games/$g"; done)"
check "no pictures but the README's, no sound or video" "$(grep -iE '\.(png|jpe?g|gif|bmp|ppm|tga|webp|mp4|mov|webm|wav|ogg|mp3|flac)$' "$LIST" | grep -vE '^docs/screenshots/([a-z0-9_]+/)?[a-z0-9_-]+\.(png|jpg|gif)$' | grep -vx 'recomp-common/launcher/flags\.png' || true)"
# (the hashes of the files recomp-common's copy holds, and the checksums of what the builds download, are not keys)
check "no key-like hex strings (32 digits or more)" "$(grep_files '(^|[^0-9a-fA-F.])[0-9a-fA-F]{32,}([^0-9a-fA-F]|$)' | grep -vE 'URL_HASH SHA256=[0-9a-f]{64}|SHA256=[0-9a-f]{64}|_SHA256: [0-9a-f]{64}|_COMMIT=[0-9a-f]{40}$|^\.github/workflows/' \
    | grep -vE '^games/[^/]+/game\.cfg:[0-9]+:cipher_sha1 = [0-9a-f]{40}$' || true)"
# (a key or a cipher's table written out as a list of numbers: 256 or more byte-sized hex numbers in one {...} or
# [...], C's or Python's. The PGM games' tables are worked out from the player's own ROM, machine/pgm_crypt.cpp, and
# game.cfg has only their SHA-1, above. port/shaders.inc is the SPIR-V tools/gen_shaders.py compiles our shaders to)
check "no key-like tables (256 byte values or more in a list) outside third_party/" "$(grep -vE "$others" "$LIST" | grep -vx 'port/shaders\.inc' | while read -r f; do \
    [ -f "$f" ] && grep -qI . "$f" && echo "$f"; done | tr '\n' '\0' | LC_ALL=C xargs -0 awk '
    function look(text, name,   block, n) {
        while (match(text, /[{[][[:space:]0-9A-Fa-fxXuU,]*[]}]/)) {
            block = substr(text, RSTART, RLENGTH); text = substr(text, RSTART + RLENGTH)
            n = gsub(/0[xX][0-9A-Fa-f][0-9A-Fa-f]?[uU]?[^0-9A-Fa-fxX]/, "&", block)
            if (n >= 256) { print name ": a list of " n " byte values"; return }
        }
    }
    FNR == 1 && NR > 1 { look(all, last) }
    FNR == 1 { all = ""; last = FILENAME }
    { all = all "\n" $0 }
    END { if (NR) look(all, last) }' || true)"
# (the shadow build's reference model, tools/shadow/: kept exactly as the model was before its hot path changed)
check "the timing model's reference is unchanged (machine/bus_timing_ref.h)" "$(sum=$( (sha256sum machine/bus_timing_ref.h 2>/dev/null || shasum -a 256 machine/bus_timing_ref.h) | cut -c1-64); \
    [ "SHA256=$sum" = "SHA256=e9f69a458c67f7b5c5da915f2d4fe9bee3bbf7adf980303828936b11c4c2b108" ] || echo "machine/bus_timing_ref.h: $sum")"
check "no home folder or machine paths" "$(grep_files -e '/Users/[a-zA-Z]' -e '/home/[a-z]+/' -e 'C:\\\\Users\\\\[a-zA-Z]' -e '~/code/' -e '~/\.claude' -e '/Volumes/' -e '/private/tmp/claude' -e '/tmp/claude-' || true)"
check "no private session links or private network addresses" "$(grep_files -e 'claude\.ai/code/session' -e '\b192\.168\.[0-9]+\.[0-9]+' -e '\b10\.[0-9]+\.[0-9]+\.[0-9]+\b' -e '\.local\b[^/_]' || true)"
# (third_party/'s files are kept as their projects publish them: up to 1 MB; the README's pictures up to 6 MB, its
# animated GIFs up to 10 MB: a few seconds of a busy scene at 30 pictures a second; GitHub shows images up to 10 MB)
# (the files found over SIZE KB, of the paths read; xargs hands find the paths before its tests)
big() { tr '\n' '\0' | xargs -0 sh -c 's=$1; shift; find "$@" -prune -type f -size +"$s"k' sh "$1" || true; }
check "no file over 512 KB (third_party/: 1 MB; docs/screenshots/: 6 MB, its GIFs 10 MB)" "$(grep -vE '^(docs/screenshots|third_party)/' "$LIST" | big 512; \
    grep '^third_party/' "$LIST" | big 1024; grep '^docs/screenshots/' "$LIST" | grep -v '\.gif$' | big 6144; \
    grep '^docs/screenshots/.*\.gif$' "$LIST" | big 10240)"
# (the public repository may name only public ports: cps2recomp, satrecomp, recomp-common, Fighting Vipers, Fighters
# Megamix, Soulcalibur II, Vampire Savior, Muscle March. Add a port here while it is private; take it out once public)
check "no names of ports that are still private" "$(grep_files_i -e 'sf3comp' -e 'cps3recomp' -e 'naomi_?comp' -e 'naomirecomp' -e 'snesrecomp' -e 'bonkers' \
    -e 'puzzle-fighter-recomp' -e 'sfa2-recomp' -e 'doaxv?b' -e 'doa2u' -e 'recomp-tools' -e 'tmcleroy/attract' -e 'github\.com/tmcleroy/spelunk([^o]|$)' || true)"
check "no words that point to where game files or keys are found" "$(grep_files_i -e 'archive\.org' -e 'internet archive' -e '\bIA item' -e 'vintagearcade' -e 'rom ?sites?\b' -e 'emuparadise' \
    -e 'vimm' -e 'romsfun' -e 'romspedia' -e 'coolrom' -e 'torrent' -e 'pleasuredome' -e 'download (the|a|an|your)? ?(rom|roms|rom set|rom sets|key|bios)\b' || true)"
# (the boards' makers' own tools and libraries: nothing from them may be here, nor their names as if it were)
check "no SDK or leaked material named" "$(grep_files -e '\bXDK\b' -e 'Katana SDK' -e '\bSHC\b' -e 'Hitachi C compiler' -e 'CRIWARE' -e '[Ll]eaked (source|code|SDK)' | grep -v '^CONTRIBUTING\.md:' || true)"
check "no submodules" "$(git ls-files -s | awk '$1 == "160000" {print $4}'; [ ! -f .gitmodules ] || echo .gitmodules)"
check "no patch files" "$(grep -iE '\.(patch|diff)$|(^|/)patches/' "$LIST" || true)"
check "no emulator's sources in the tree" "$(grep -iE '(^|/)(vendor|external|deps)/|(^|/)(fbneo|fbalpha|finalburn|musashi|libretro|mame)[^/]*/' "$LIST" || true)"
# Every file of another project's is in third_party/ and named in the notices (by its path, its name or its name
# without the ending: `ymfm_opn.*`)
check "third_party/ holds only what the notices list" "$(grep '^third_party/' "$LIST" | while read -r f; do b=$(basename "$f"); s=${b%.*}; \
    grep -qF -e "$f" -e "\`$b\`" -e "\`$s.*\`" THIRD_PARTY_NOTICES.md || { [ "$f" != "${f%.h}" ] && grep -qF "\`third_party/${f#third_party/}" THIRD_PARTY_NOTICES.md; } || \
    grep -qF "\`${f%.*}.cpp\` and \`.h\`" THIRD_PARTY_NOTICES.md || echo "$f"; done)"
# (a file of ours that follows another project names it in its first lines, or carries another licence: the
# notices must name the file, as path/name.* or path/name.ext)
check "files that name another project in their first lines are in the notices" "$(grep -vE "$others" "$LIST" | grep -E '\.(h|c|cpp|inc|py|frag|vert)$' | while read -r f; do \
    head -12 "$f" | grep -qE 'BSD-3-Clause|FBNeo|FinalBurn|Musashi|libretro|license: *BSD|SPDX-License-Identifier: (BSD|MIT|Zlib|GPL-2|LGPL|MPL)' || continue; \
    grep -qF -e "$f" -e "${f%.*}.*" -e "${f%.*}\`" THIRD_PARTY_NOTICES.md || echo "$f"; done)"
# (third_party/'s files have their own licences, and recomp-common/ its own lines)
check "every source file has its license and copyright lines" "$(grep -vE "$others" "$LIST" | grep -E '\.(h|c|cpp|inc|ipp|frag|vert|py|sh|mk|cmake|cfg|yml)$|CMakeLists\.txt$|(^|/)Makefile$|(^|/)Dockerfile$' | while read -r f; do \
    head -5 "$f" | grep -q 'SPDX-License-Identifier: GPL-3.0-only' && head -5 "$f" | grep -q 'Copyright (C) 20[0-9][0-9] ' || echo "$f"; done)"
check "the README and the notices are there" "$(for f in README.md LICENSE THIRD_PARTY_NOTICES.md CONTRIBUTING.md CODE_OF_CONDUCT.md SECURITY.md CHANGELOG.md VERSION; do [ -f "$f" ] || echo "missing: $f"; done)"
[ "$fail" = 0 ] || { echo "audit failed"; exit 1; }
echo "audit passed ($(wc -l < "$LIST" | tr -d ' ') files)"
