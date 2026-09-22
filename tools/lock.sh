#!/bin/bash
# ENGINE behaviour lock. Usage: tools/lock.sh <tag>   (writes /tmp/lock_<tag>/)
# Not part of the build; a scratch harness for the refactor.
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TAG="${1:-x}"
OUT="/tmp/lock_$TAG"
mkdir -p "$OUT"
cd "$ROOT" || exit 1
FAIL=0
for S in --chapter-playtest --chapter-logic-selftest --clips-selftest --battle-selftest \
         --battle-ui-test --vox-selftest --robustness; do
    if ./capture.sh $S > "$OUT/${S#--}.log" 2>&1; then echo "ok   $S"; else echo "FAIL $S"; FAIL=1; fi
done
if ./capture.sh --vox-walktest all > "$OUT/vox-walktest.log" 2>&1; then echo "ok   --vox-walktest all"; else echo "FAIL --vox-walktest all"; FAIL=1; fi

./capture.sh --vox halm --at 24,18 --name lock_a > "$OUT/img_a.log" 2>&1
./capture.sh --vox high_pasture --light night:0.16 --lantern --name lock_b > "$OUT/img_b.log" 2>&1
./capture.sh --dialog 0110_the_board 4 > "$OUT/dialog.log" 2>&1
for f in build_desktop/vox_halm_lock_a.png build_desktop/vox_high_pasture_lock_b.png \
         build_desktop/dialog_0110_the_yard_l4_p0.png; do
    [ -f "$f" ] && cp "$f" "$OUT/$(basename $f)"
done
( cd "$OUT" && md5 -q *.png 2>/dev/null > md5.txt; md5 -q *.png 2>/dev/null )
echo "images: $(cat "$OUT/md5.txt" 2>/dev/null | tr '\n' ' ')"
echo "dialog: $(md5 -q <(grep -v '^$' "$OUT/dialog.log"))"
exit $FAIL
