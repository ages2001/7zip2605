#!/usr/bin/env bash
# Build production components in dependency order. This never publishes files.
set -euo pipefail
nt9x_source=$(cd "$(dirname "$0")/../.." && pwd)
nt9x_build=${NT9X_BUILD:-/workspace/nt9x-build}
cd "$nt9x_source"
bash Compat/NT9x/build_support.sh
bash Compat/NT9x/build_cli_objects.sh
bash Compat/NT9x/link_i386.sh
python3 Compat/NT9x/generate_gui_makefile.py
make -C "$nt9x_build/gui-i386" -j"${NT9X_JOBS:-4}"
bash Compat/NT9x/link_gui_i386.sh
bash Compat/NT9x/build_components.sh
bash Compat/NT9x/build_winfile_extension.sh
bash Compat/NT9x/build_installer.sh
if [[ ${NT9X_BUILD_TESTS:-1} == 1 ]]; then
 bash Compat/NT9x/build_tests.sh
 bash Compat/NT9x/build_gui_roundtrip_tests.sh
 bash Compat/NT9x/build_component_tests.sh
 bash Compat/NT9x/build_icon_tests.sh
 bash Compat/NT9x/build_shell_tests.sh
 bash Compat/NT9x/build_plugin_fixture.sh
 bash Compat/NT9x/build_gui_drop_tests.sh
 bash Compat/NT9x/build_unicode_setup_tests.sh
 bash Compat/NT9x/build_ole_tests.sh
 bash Compat/NT9x/build_mpr_tests.sh
 bash Compat/NT9x/build_listview_tests.sh
 bash Compat/NT9x/build_shortcut_tests.sh
 bash Compat/NT9x/build_regression_runner.sh
 bash Compat/NT9x/build_link_metadata_tests.sh
 bash Compat/NT9x/build_gui_help_tests.sh
 bash Compat/NT9x/build_short_tests.sh
 bash Compat/NT9x/build_association_tests.sh
fi
