#!/usr/bin/env bash
set -euo pipefail
: "${WATCOM:?Set WATCOM to an extracted Open Watcom installation}"
win16_source=$(cd "$(dirname "$0")/../.." && pwd)
win16_build=${WIN16_BUILD:-/workspace/nt9x-tests/win16/gui}
mkdir -p "$win16_build"
export PATH="$WATCOM/binl64:$PATH"
export INCLUDE="$WATCOM/h:$WATCOM/h/win"
cd "$win16_build"
win16_units=()
for win16_unit in 7zArcIn 7zBuf 7zStream 7zCrc; do
 win16_units+=("$win16_source/C/$win16_unit.c")
done
win16_flags=(-q -zu -os -bt=windows -zW -ml -0 -dZ7_DECL_Int32_AS_long -dZ7_WIN16 -dZ7_WIN16_PAGED_DICT -dZ7_ST -dZ7_CRC_NUM_TABLES=1 -dZ7_CRC64_NUM_TABLES=1)
# Windows can move code while a callback pumps messages. All far functions
# need Windows stack frames; -zW preserves the exported callback DS prologues.
# Exported callbacks must not receive -of/-of+, which remove their DS prologues.
wcc "${win16_flags[@]}" ${WIN16_GUI_FLAGS:-} -fo=file_manager.o "$win16_source/Compat/Win16/file_manager.c"
wcc "${win16_flags[@]}" -of+ -nc=TOOLBAR_FAST_CODE ${WIN16_GUI_FLAGS:-} -fo=toolbar_fast.o "$win16_source/Compat/Win16/toolbar_fast.c"
# Hash algorithms are substantial compared with the real-mode memory budget.
# Let Windows load their code only for checksums and discard it afterwards.
wcc "${win16_flags[@]}" -of+ -nc=FS_OPS_CODE ${WIN16_GUI_FLAGS:-} -fo=filesystem_ops.o "$win16_source/Compat/Win16/filesystem_ops.c"
win16_hash_objects=(hash_file.o filesystem_ops.o)
wcc "${win16_flags[@]}" -of+ -nc=HASH_CODE ${WIN16_GUI_FLAGS:-} -fo=hash_file.o "$win16_source/Compat/Win16/hash_file.c"
for win16_unit in XzCrc64 Md5 Sha1 Sha256 Sha512 Sha3 Blake2s Xxh64; do
 wcc "${win16_flags[@]}" -of+ -nc=HASH_CODE ${WIN16_GUI_FLAGS:-} -fo="$win16_unit.o" "$win16_source/C/$win16_unit.c"
 win16_hash_objects+=("$win16_unit.o")
done
# Branch converters are only needed for filtered data. Keeping all of them
# resident consumes scarce conventional memory even during plain LZMA updates
# in Windows 3.0 real mode. Direct calls and Windows stack frames let the loader
# retain active converters and discard inactive ones safely.
win16_filter_objects=()
for win16_unit in Bcj2 Bra Bra86 BraIA64 Delta; do
 wcc "${win16_flags[@]}" -of+ -nc=FILTER_CODE ${WIN16_GUI_FLAGS:-} -fo="$win16_unit.o" "$win16_source/C/$win16_unit.c"
 win16_filter_objects+=("$win16_unit.o")
done
# Put codecs in separate load-on-call classes instead of the large preloaded
# code class. Keep them resident after loading: discarding these codec segments
# caused real-mode return failures during BCJ2 extraction and LZMA updates.
# Windows frames are still required when message callbacks move loaded code.
win16_codec_objects=()
for win16_unit in 7zDec Lzma2Dec LzmaDec Aes; do
 wcc "${win16_flags[@]}" -of+ -nc=ARCHIVE_DEC_CODE ${WIN16_GUI_FLAGS:-} -fo="$win16_unit.o" "$win16_source/C/$win16_unit.c"
 win16_codec_objects+=("$win16_unit.o")
done
wcc "${win16_flags[@]}" -of+ -nc=ARCHIVE_ENC_CODE ${WIN16_GUI_FLAGS:-} -fo=LzmaEnc.o "$win16_source/C/LzmaEnc.c"
win16_codec_objects+=(LzmaEnc.o)
# Force frames even for backend leaf routines: Windows stack walking must not
# mistake saved registers for return addresses when loading another segment.
# These units contain no exported window callbacks.
wcl "${win16_flags[@]}" -of+ ${WIN16_GUI_FLAGS:-} -l=windows -fe=7ZFM16.EXE -fd=file_manager.lnk \
 file_manager.o toolbar_fast.o "${win16_hash_objects[@]}" "${win16_filter_objects[@]}" "$win16_source/Compat/Win16/paged_dictionary.c" "$win16_source/Compat/Win16/stream_folder.c" "$win16_source/Compat/Win16/deflate_stream.c" "$win16_source/Compat/Win16/deflate_encode.c" "$win16_source/Compat/Win16/zip_archive.c" \
 "$win16_source/Compat/Win16/lowmem_match_finder.c" "${win16_codec_objects[@]}" "${win16_units[@]}"
# Bound packing so algorithms can be discarded without a single large locked
# driver block retaining most of the checksum engine in conventional memory.
# Keep the established 8 KiB stack; runtime stack checks remain enabled.
# The UI integration driver is load-on-call but is not discarded while its
# archive-test call is active. Windows 3.0 real-mode return-thunk failures were
# observed when this test-only caller was discarded during nested GUI work.
# Normal interactive use never loads UI_TEST_TEXT or FS_TEST_TEXT. The filesystem
# drivers explicitly lock their segments only for the duration of each call.
# Keep the batch driver in a distinct shared code block: packing it with the
# browser driver needlessly retains it during the real-mode Deflate test.
printf '%s\n' 'option packcode=16384' 'option stack=8192' 'segment BCJ2_STREAM_TEXT loadoncall moveable discardable' 'segment class TOOLBAR_FAST_CODE loadoncall moveable discardable' 'segment class HASH_CODE loadoncall moveable discardable' 'segment class FILTER_CODE loadoncall moveable discardable' 'segment class ARCHIVE_DEC_CODE loadoncall moveable' 'segment class ARCHIVE_ENC_CODE loadoncall moveable' 'segment FS_VIEW_TEXT loadoncall moveable discardable' 'segment FS_TEST_TEXT loadoncall moveable discardable' 'segment BATCH_TEST_TEXT loadoncall moveable discardable shared' 'segment SELECT_TEST_TEXT loadoncall moveable shared' 'segment class D64_TEST_CODE loadoncall moveable discardable' 'segment UI_TEST_TEXT loadoncall moveable' 'segment HASH_TEST_TEXT loadoncall moveable' 'segment FS_GUI_TEXT loadoncall moveable discardable' 'segment class FS_OPS_CODE loadoncall moveable discardable' >> file_manager.lnk
wlink @file_manager.lnk

(cd "$win16_source/Compat/Win16"
 wrc -q -30 -s0 -bt=windows -fo="$win16_build/file_manager.res" file_manager.rc "$win16_build/7ZFM16.EXE")
