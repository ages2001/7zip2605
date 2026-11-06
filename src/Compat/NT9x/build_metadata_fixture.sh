#!/usr/bin/env bash
set -euo pipefail
# Create only a new, disposable filesystem image; never operate on guest VHDs.
nt9x_tools=${NT9X_TOOLS:-/workspace/nt9x-tools}
nt9x_fixture=${NT9X_METADATA_FIXTURE:-/workspace/nt9x-tests/metadata}
mkdir -p "$nt9x_fixture"
nt9x_image=$(mktemp "$nt9x_fixture/fixture.XXXXXX.ntfs")
trap 'rm -f "$nt9x_image"' EXIT
export LD_LIBRARY_PATH="$nt9x_tools/root/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
truncate -s 32M "$nt9x_image"
"$nt9x_tools/root/usr/sbin/mkntfs" -F -Q -L Z7META "$nt9x_image" > "$nt9x_fixture/mkntfs.log" 2>&1
printf 'Metadata preservation fixture.\r\n' > "$nt9x_fixture/DATA.TXT"
"$nt9x_tools/root/usr/sbin/ntfscp" -f "$nt9x_image" "$nt9x_fixture/DATA.TXT" /F
"$nt9x_tools/root/usr/sbin/ntfscp" -f -N S "$nt9x_image" "$nt9x_fixture/DATA.TXT" /F
nt9x_wim=$(mktemp "$nt9x_fixture/capture.XXXXXX.wim")
rm "$nt9x_wim"
"$nt9x_tools/root/usr/bin/wimcapture" "$nt9x_image" "$nt9x_wim" 'Metadata fixture' --compress=none > "$nt9x_fixture/capture.log" 2>&1
mv "$nt9x_wim" "$nt9x_fixture/META.WIM"
